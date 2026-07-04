"""c2 — command & control host (the register 'master').

Sends a single ReadRegister / WriteRegister request and prints the decoded reply.
Correlation is implicit request/response (`link.request`), so this is just:
send → await reply → print.

Run from the repo root as a module:
    python -m apps.c2 --tcp 127.0.0.1:5555 read 0x10 4
    python -m apps.c2 --port /dev/ttyUSB0 write 0x20 0x1234
"""

import argparse

from serial_link import FramedLink, ReadRegister, WriteRegister, ReadResponse, WriteAck, Nak
from serial_link.cli import add_transport_args, open_transport

from apps.c2_log import C2CsvLogger


def _format_reply(message):
    if isinstance(message, ReadResponse):
        vals = "  ".join(f"[{message.addr + i:#06x}]=0x{v:04X}"
                         for i, v in enumerate(message.values))
        return f"OK  read {len(message.values)} reg(s): {vals}"
    if isinstance(message, WriteAck):
        return f"OK  wrote [{message.addr:#06x}]=0x{message.value:04X} (status {message.status})"
    if isinstance(message, Nak):
        return f"NAK addr={message.addr:#06x}: {message.reason()}"
    return f"unexpected reply: {message!r}"


def main(argv=None):
    parser = argparse.ArgumentParser(description="Register command & control host.")
    add_transport_args(parser)
    parser.add_argument("--timeout", type=float, default=1.0,
                        help="seconds to wait for the reply (default: 1.0)")
    parser.add_argument("--log", metavar="FILE",
                        help="append the transaction to a CSV audit log")
    sub = parser.add_subparsers(dest="op", required=True)
    p_read = sub.add_parser("read", help="read register(s)")
    p_read.add_argument("addr", help="start address (e.g. 0x10)")
    p_read.add_argument("count", nargs="?", default="1", help="how many (default 1)")
    p_write = sub.add_parser("write", help="write a register")
    p_write.add_argument("addr", help="address (e.g. 0x20)")
    p_write.add_argument("value", help="value (e.g. 0x1234)")
    args = parser.parse_args(argv)

    if args.op == "read":
        request = ReadRegister(addr=int(args.addr, 0), count=int(args.count, 0))
    else:
        request = WriteRegister(addr=int(args.addr, 0), value=int(args.value, 0))

    transport = open_transport(args)
    link = FramedLink(transport)
    try:
        reply = link.request(request, timeout=args.timeout)
    finally:
        link.close()

    # Decide outcome, then log it (host perspective) before printing.
    if reply is None:
        reply_msg, result = None, "timeout"
    elif not reply.ok:
        reply_msg, result = None, "error"
    else:
        reply_msg, result = reply.message, None

    if args.log:
        with C2CsvLogger(args.log) as log:
            log.log_txn("host", request, reply_msg, result=result)

    if reply is None:
        print(f"timeout after {args.timeout:g}s — no reply")
        return 1
    if not reply.ok:
        print(f"bad reply frame: {reply.error}")
        return 1
    print(_format_reply(reply.message))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
