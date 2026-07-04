import csv

from serial_link import AttitudeSample
from apps.attitude_log import AttitudeCsvLogger


def test_logger_writes_header_and_rows(tmp_path):
    path = tmp_path / "attitude.csv"
    samples = [
        AttitudeSample(seq=0, t_ms=0, roll=1.0, pitch=-2.0, yaw=3.0),
        AttitudeSample(seq=1, t_ms=100, roll=12.5, pitch=-3.25, yaw=180.0),
    ]
    with AttitudeCsvLogger(str(path)) as log:
        for i, s in enumerate(samples):
            log.write(s, rx_unix=1000.0 + i)
        assert log.count == 2

    with open(path, newline="") as f:
        rows = list(csv.DictReader(f))

    assert [r["seq"] for r in rows] == ["0", "1"]
    assert rows[1]["t_ms"] == "100"
    assert float(rows[1]["roll"]) == 12.5
    assert float(rows[1]["yaw"]) == 180.0
    assert rows[0]["rx_unix"] == "1000.000000"


def test_rows_flushed_before_close(tmp_path):
    # Each write flushes, so data survives even without a clean close.
    path = tmp_path / "partial.csv"
    log = AttitudeCsvLogger(str(path))
    log.write(AttitudeSample(seq=5, t_ms=500, roll=0, pitch=0, yaw=0))
    # do NOT close — read what's on disk
    with open(path, newline="") as f:
        rows = list(csv.DictReader(f))
    assert len(rows) == 1 and rows[0]["seq"] == "5"
    log.close()
