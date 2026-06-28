"""attitude_publisher — replay recorded attitude out over the link.

The inverse of ``monitor``: read attitude samples from a CSV file and stream them
out as ``AttitudeSample`` frames over RS-422 (or a stand-in transport). Reads the
same CSV format ``monitor --log`` writes, so a recording round-trips.

Pacing:
  * default — fixed ``--rate`` Hz (drift-corrected), like the sensor.
  * --realtime — honour the recorded ``t_ms`` timing (gaps and all). Requires a
    ``t_ms`` column.

Run from the repo root as a module:
    python -m apps.attitude_publisher --port /dev/ttyUSB0 attitude.csv
    python -m apps.attitude_publisher --tcp 127.0.0.1:5555 --realtime flight.csv
    python -m apps.attitude_publisher --pty --loop attitude.csv
"""

import argparse
import time

from serial_link import FramedLink, AttitudeSample
from serial_link.cli import add_transport_args, open_transport

from apps.attitude_log import read_attitude_csv

U32 = 0xFFFFFFFF


def publish(link, rows, rate=10.0, realtime=False, loop=False, count=None,
            pace=True, address=None):
    """Stream ``rows`` (list of AttitudeRow) out over ``link``.

    Returns the number of samples emitted. ``pace=False`` skips all sleeping
    (used by tests). seq is taken from the file for a faithful single-pass replay,
    but switches to a fresh monotonic counter when ``loop`` is set (so it never
    repeats). Missing ``t_ms`` is filled from the nominal rate.
    """
    if realtime and any(r.t_ms is None for r in rows):
        raise ValueError("--realtime needs a 't_ms' column on every row")

    period = 1.0 / rate
    start = time.monotonic()
    emitted = 0
    while True:
        prev_t_ms = None
        for row in rows:
            if count is not None and emitted >= count:
                return emitted

            seq = emitted if (loop or row.seq is None) else row.seq
            t_ms = row.t_ms if row.t_ms is not None else round(emitted / rate * 1000.0)
            msg = AttitudeSample(
                seq=seq & U32, t_ms=t_ms & U32,
                roll=row.roll, pitch=row.pitch, yaw=row.yaw,
            )
            if address is None:
                link.send(msg)
            else:
                link.send(msg, address=address)
            emitted += 1

            if pace:
                if realtime:
                    # delta-based: honour the recorded inter-sample interval
                    if prev_t_ms is not None:
                        dt = (row.t_ms - prev_t_ms) / 1000.0
                        if dt > 0:
                            time.sleep(dt)
                    prev_t_ms = row.t_ms
                else:
                    target = start + emitted * period
                    dt = target - time.monotonic()
                    if dt > 0:
                        time.sleep(dt)
        if not loop:
            return emitted


def main(argv=None):
    parser = argparse.ArgumentParser(description="Replay recorded attitude over the link.")
    add_transport_args(parser)
    parser.add_argument("csv", help="CSV file of attitude samples (roll/pitch/yaw required)")
    parser.add_argument("--rate", type=float, default=10.0,
                        help="samples per second when not --realtime (default: 10)")
    parser.add_argument("--realtime", action="store_true",
                        help="pace using the recorded t_ms timing instead of --rate")
    parser.add_argument("--loop", action="store_true",
                        help="repeat the file until stopped")
    parser.add_argument("--count", type=int, default=None,
                        help="stop after N samples")
    args = parser.parse_args(argv)

    rows = read_attitude_csv(args.csv)
    transport = open_transport(args)
    link = FramedLink(transport)
    mode = "realtime" if args.realtime else f"{args.rate:g} Hz"
    print(f"[publisher] replaying {len(rows)} sample(s) from {args.csv} "
          f"({mode}{', looping' if args.loop else ''}) — Ctrl-C to stop", flush=True)
    sent = 0
    try:
        sent = publish(link, rows, rate=args.rate, realtime=args.realtime,
                       loop=args.loop, count=args.count)
    except KeyboardInterrupt:
        pass
    finally:
        link.close()
        print(f"\n[publisher] stopped after {sent} sample(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
