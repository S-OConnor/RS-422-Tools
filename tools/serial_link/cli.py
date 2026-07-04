"""Shared CLI plumbing: turn ``--port/--tcp/--pty`` flags into a Transport.

Reused by every serial-side tool (and by the ethernet bridge) so the
transport-selection UX stays identical everywhere.
"""

from serial_link import SerialTransport, LoopbackTransport, PtyTransport


def add_transport_args(parser):
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--port", metavar="DEV",
                       help="serial device, e.g. /dev/ttyUSB0 (the FTDI cable)")
    group.add_argument("--tcp", metavar="HOST:PORT",
                       help="TCP endpoint, e.g. 127.0.0.1:5555")
    group.add_argument("--pty", action="store_true",
                       help="create a local pty pair and print the slave path")
    parser.add_argument("--baud", type=int, default=115200,
                        help="serial baud rate (default: 115200)")
    parser.add_argument("--listen", action="store_true",
                        help="with --tcp, listen for a connection instead of dialing")


def open_transport(args):
    """Build a Transport from parsed args. Prints the pty slave path if used."""
    if args.port:
        return SerialTransport(args.port, baud=args.baud)
    if args.tcp:
        host, _, port = args.tcp.partition(":")
        port = int(port)
        if args.listen:
            print(f"[listening on {host}:{port} ...]", flush=True)
            return LoopbackTransport.listen_one(host, port)
        return LoopbackTransport.connect(host, port)
    transport = PtyTransport()
    print(f"[pty ready] point the peer at: {transport.slave_name}", flush=True)
    return transport
