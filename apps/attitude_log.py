"""CSV read/write for the attitude stream.

`AttitudeCsvLogger` records each received ``AttitudeSample`` as a row, with the
wall-clock receive time alongside the sensor's own ``t_ms`` so arrival
cadence/jitter can be analysed later against the nominal 10 Hz. Rows are flushed
as they are written so a Ctrl-C (or crash) keeps everything up to the last sample.

`read_attitude_csv` parses that same format back into rows for replay (see
``apps.attitude_publisher``). Only ``roll``/``pitch``/``yaw`` are required;
``seq`` and ``t_ms`` are optional and come back as ``None`` when absent.
"""

import csv
import time
from collections import namedtuple

# A parsed CSV row. seq / t_ms are None when the column is missing or blank.
AttitudeRow = namedtuple("AttitudeRow", ["seq", "t_ms", "roll", "pitch", "yaw"])

REQUIRED_COLUMNS = ("roll", "pitch", "yaw")


class AttitudeCsvLogger:
    """Append attitude samples to a CSV file (header written on open)."""

    FIELDS = ["rx_unix", "seq", "t_ms", "roll", "pitch", "yaw"]

    def __init__(self, path):
        self.path = path
        self._f = open(path, "w", newline="")
        self._w = csv.writer(self._f)
        self._w.writerow(self.FIELDS)
        self._f.flush()
        self.count = 0

    def write(self, sample, rx_unix=None):
        """Write one sample row. ``rx_unix`` defaults to the current wall clock."""
        if rx_unix is None:
            rx_unix = time.time()
        self._w.writerow([
            f"{rx_unix:.6f}",
            sample.seq,
            sample.t_ms,
            f"{sample.roll:.4f}",
            f"{sample.pitch:.4f}",
            f"{sample.yaw:.4f}",
        ])
        self._f.flush()
        self.count += 1

    def close(self):
        try:
            self._f.flush()
            self._f.close()
        except (OSError, ValueError):
            pass

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()


def read_attitude_csv(path):
    """Read a CSV into a list of :class:`AttitudeRow`.

    Requires ``roll``/``pitch``/``yaw`` columns; ``seq``/``t_ms`` are optional.
    Raises ``ValueError`` on missing required columns or an empty file.
    """
    def _opt_int(value):
        return int(value) if value not in (None, "") else None

    with open(path, newline="") as f:
        reader = csv.DictReader(f)
        fields = set(reader.fieldnames or [])
        missing = [c for c in REQUIRED_COLUMNS if c not in fields]
        if missing:
            raise ValueError(f"CSV {path!r} is missing required column(s): {missing}")
        rows = [
            AttitudeRow(
                seq=_opt_int(r.get("seq")),
                t_ms=_opt_int(r.get("t_ms")),
                roll=float(r["roll"]),
                pitch=float(r["pitch"]),
                yaw=float(r["yaw"]),
            )
            for r in reader
        ]
    if not rows:
        raise ValueError(f"CSV {path!r} has no data rows")
    return rows
