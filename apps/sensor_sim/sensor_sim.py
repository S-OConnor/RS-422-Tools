"""sensor_sim — stand-in for the attitude sensor.

Streams ``AttitudeSample`` frames at a fixed rate (default 10 Hz) with a gentle
motion model so a monitor's display visibly moves. A real sensor swaps in later;
nothing downstream changes.

Run from the repo root as a module:
    python -m apps.sensor_sim --tcp 127.0.0.1:5555         # dial a listening monitor
    python -m apps.sensor_sim --pty                        # prints a /dev/pts/N to read
    python -m apps.sensor_sim --port /dev/ttyUSB0          # real cable
"""

import argparse
import math
import time

from serial_link import FramedLink, AttitudeSample
from serial_link.cli import add_transport_args, open_transport

U32 = 0xFFFFFFFF


def attitude_at(t):
    """Pure motion model: attitude (deg) as a function of elapsed seconds ``t``.

    Deterministic so tests can recompute expected values. Yaw ramps and wraps;
    roll/pitch are slow out-of-phase sinusoids.
    """
    yaw = (20.0 * t) % 360.0
    pitch = 10.0 * math.sin(2.0 * math.pi * 0.10 * t)
    roll = 15.0 * math.sin(2.0 * math.pi * 0.07 * t)
    return roll, pitch, yaw


def stream(link, rate=10.0, count=None, pace=True, address=None):
    """Send ``count`` AttitudeSamples (or forever if None) at ``rate`` Hz.

    ``pace=False`` skips the inter-sample sleep — used by tests to run flat out.
    Sample ``n`` uses the nominal timestamp ``t = n / rate`` (not wall-clock), so
    the stream is reproducible regardless of pacing.
    """
    period = 1.0 / rate
    start = time.monotonic()
    n = 0
    while count is None or n < count:
        t = n / rate
        roll, pitch, yaw = attitude_at(t)
        msg = AttitudeSample(
            seq=n & U32,
            t_ms=round(t * 1000.0) & U32,
            roll=roll,
            pitch=pitch,
            yaw=yaw,
        )
        if address is None:
            link.send(msg)
        else:
            link.send(msg, address=address)
        n += 1
        if pace:
            target = start + n * period
            dt = target - time.monotonic()
            if dt > 0:
                time.sleep(dt)
    return n


def main(argv=None):
    parser = argparse.ArgumentParser(description="Stream simulated attitude samples.")
    add_transport_args(parser)
    parser.add_argument("--rate", type=float, default=10.0,
                        help="samples per second (default: 10)")
    parser.add_argument("--count", type=int, default=None,
                        help="number of samples to send (default: run forever)")
    args = parser.parse_args(argv)

    transport = open_transport(args)
    link = FramedLink(transport)
    print(f"[sensor] streaming attitude @ {args.rate:g} Hz (Ctrl-C to stop)", flush=True)
    sent = 0
    try:
        sent = stream(link, rate=args.rate, count=args.count)
    except KeyboardInterrupt:
        pass
    finally:
        link.close()
        print(f"\n[sensor] stopped after {sent} sample(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
