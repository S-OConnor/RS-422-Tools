"""monitor — receive the attitude stream and show it live.

Decodes ``AttitudeSample`` frames off the link and refreshes a single terminal
line in place with the current attitude, sample count, and the measured rate
(which should sit near the sensor's 10 Hz). Bad-FCS / unknown frames are counted
and skipped.

Optionally records every sample to CSV with ``--log FILE``.

Run from the repo root as a module:
    python -m apps.monitor --tcp 127.0.0.1:5555 --listen
    python -m apps.monitor --pty
    python -m apps.monitor --port /dev/ttyUSB0 --log attitude.csv
"""

import argparse
import time

from serial_link import FramedLink, AttitudeSample
from serial_link.cli import add_transport_args, open_transport

from apps.attitude_log import AttitudeCsvLogger


def format_line(sample, rate_hz, drops):
    """One-line attitude readout (no trailing newline)."""
    drop_str = f"  drops={drops}" if drops else ""
    return (
        f"roll={sample.roll:+7.1f}°  pitch={sample.pitch:+7.1f}°  "
        f"yaw={sample.yaw:6.1f}°   seq={sample.seq:<6d} rate={rate_hz:4.1f}Hz{drop_str}"
    )


def main(argv=None):
    parser = argparse.ArgumentParser(description="Live-display the attitude stream.")
    add_transport_args(parser)
    parser.add_argument("--log", metavar="FILE",
                        help="also record every sample to a CSV file")
    args = parser.parse_args(argv)

    transport = open_transport(args)
    link = FramedLink(transport)
    logger = AttitudeCsvLogger(args.log) if args.log else None
    if logger:
        print(f"[monitor] logging samples to {args.log}", flush=True)
    print("[monitor] waiting for attitude (Ctrl-C to stop)", flush=True)

    count = 0
    drops = 0
    errors = 0
    expected_seq = None
    rate_hz = 0.0
    last_t = None
    try:
        for rf in link.receive():
            if not rf.ok or not isinstance(rf.message, AttitudeSample):
                errors += 1
                continue
            sample = rf.message
            count += 1
            if logger:
                logger.write(sample)

            # gap detection from the seq counter
            if expected_seq is not None and sample.seq != expected_seq:
                drops += (sample.seq - expected_seq) & 0xFFFFFFFF
            expected_seq = (sample.seq + 1) & 0xFFFFFFFF

            # measured rate: EMA over inter-arrival time
            now = time.monotonic()
            if last_t is not None:
                dt = now - last_t
                if dt > 0:
                    inst = 1.0 / dt
                    rate_hz = inst if rate_hz == 0.0 else 0.8 * rate_hz + 0.2 * inst
            last_t = now

            print("\r" + format_line(sample, rate_hz, drops), end="", flush=True)
    except KeyboardInterrupt:
        pass
    finally:
        link.close()
        if logger:
            logger.close()
        log_str = f", logged {logger.count} to {args.log}" if logger else ""
        print(f"\n[monitor] stopped — {count} sample(s), {drops} dropped, "
              f"{errors} bad frame(s){log_str}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
