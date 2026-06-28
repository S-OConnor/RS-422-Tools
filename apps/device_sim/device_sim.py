"""device_sim — stand-in for the controlled board (the C2 'slave').

Holds a bank of registers and answers ReadRegister / WriteRegister requests with
ReadResponse / WriteAck (or Nak). The real board replaces this later; the host
(`c2`) doesn't change.

In TCP ``--listen`` mode the device keeps its register state and re-accepts a new
connection after each client disconnects, so a sequence of one-shot ``c2``
commands all hit the same device — just like an always-on serial device. Over a
real serial port (``--port``) it simply reads forever.

Run from the repo root as a module:
    python -m apps.device_sim --tcp 127.0.0.1:5555 --listen
    python -m apps.device_sim --port /dev/ttyUSB0 --size 256 --seed 0x10=0x1234
"""

import argparse

from serial_link import FramedLink
from serial_link.cli import add_transport_args, open_transport

from apps.device_sim.registers import RegisterStore, respond
from apps.c2_log import C2CsvLogger


def _parse_seed(items):
    """Parse ['0x10=0x1234', '5=42'] into {addr: value}."""
    seed = {}
    for item in items or []:
        addr, _, value = item.partition("=")
        seed[int(addr, 0)] = int(value, 0)
    return seed


def serve(link, store, logger=None):
    """Answer requests on ``link`` until the peer disconnects (EOF).

    Returns (served, errors) for this connection. The ``store`` is supplied by
    the caller so state persists across reconnections.
    """
    served = errors = 0
    for rf in link.receive():
        if not rf.ok:
            errors += 1
            continue
        reply = respond(store, rf.message)
        if reply is not None:
            link.send(reply)
            served += 1
            if logger:
                logger.log_txn("device", rf.message, reply)
            print(f"[device] {rf.message!r} -> {reply!r}", flush=True)
    return served, errors


def main(argv=None):
    parser = argparse.ArgumentParser(description="Simulate a register device (C2 slave).")
    add_transport_args(parser)
    parser.add_argument("--size", type=int, default=256,
                        help="number of 16-bit registers (default: 256)")
    parser.add_argument("--seed", action="append", metavar="ADDR=VAL",
                        help="preset a register (repeatable), e.g. --seed 0x10=0x1234")
    parser.add_argument("--log", metavar="FILE",
                        help="append served transactions to a CSV audit log")
    args = parser.parse_args(argv)

    store = RegisterStore(size=args.size, seed=_parse_seed(args.seed))
    logger = C2CsvLogger(args.log) if args.log else None
    # TCP listeners re-accept new clients; other transports run a single session.
    reconnect = bool(args.tcp and args.listen)

    if logger:
        print(f"[device] logging transactions to {args.log}", flush=True)
    print(f"[device] {args.size} registers ready"
          f"{' (re-accepting clients)' if reconnect else ''} — Ctrl-C to stop",
          flush=True)

    total = 0
    try:
        while True:
            transport = open_transport(args)
            link = FramedLink(transport)
            try:
                served, _ = serve(link, store, logger)
            finally:
                link.close()
            total += served
            if not reconnect:
                break
    except KeyboardInterrupt:
        pass
    finally:
        if logger:
            logger.close()
        print(f"\n[device] stopped — {total} request(s) served", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
