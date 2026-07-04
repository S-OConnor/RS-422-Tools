"""Stand-ins for the cable — let serial_link run with no hardware.

Both classes implement the same :class:`Transport` interface as the real cable,
so the framing/codec/link stack above runs *byte-for-byte identically* whether
it is driven by an FTDI cable, a pseudo-terminal, or a local loopback. They are
how every test and demo runs before (or without) hardware.

  * :class:`PtyTransport`      — a local pseudo-terminal pair. ``slave_name`` is a
                                 ``/dev/pts`` path another process (or pyserial)
                                 can open exactly as if it were a serial port.
  * :class:`LoopbackTransport` — a local byte pipe built on a TCP socket. The TCP
                                 here is just a convenient on-host pipe, NOT
                                 networking — it imitates the wire. (Distinct from
                                 ``ethernet_publisher``'s UDP egress, which is a
                                 real product output.)
"""

import os
import socket

from .base import Transport


class PtyTransport(Transport):
    """A pseudo-terminal standing in for a serial port. Owns the master side."""

    def __init__(self):
        import tty

        self._master, slave = os.openpty()
        self.slave_name = os.ttyname(slave)
        # Raw mode: no echo, no line discipline mangling our binary bytes.
        tty.setraw(self._master)
        tty.setraw(slave)
        os.close(slave)
        os.set_blocking(self._master, False)

    def read(self, n=4096):
        try:
            return os.read(self._master, n)
        except BlockingIOError:
            return b""
        except OSError:
            self.eof = True
            return b""

    def write(self, data):
        return os.write(self._master, bytes(data))

    def close(self):
        try:
            os.close(self._master)
        except OSError:
            pass


class LoopbackTransport(Transport):
    """A local byte pipe over a (connected) TCP socket — a stand-in for the wire.

    Use :meth:`connect` / :meth:`listen_one` to build the two ends on localhost,
    or wrap an existing socket directly (e.g. ``socket.socketpair()`` in tests).
    """

    def __init__(self, sock):
        self._sock = sock
        self._sock.settimeout(0.1)

    @classmethod
    def connect(cls, host, port):
        return cls(socket.create_connection((host, port)))

    @classmethod
    def listen_one(cls, host, port):
        """Block until one peer connects, then return a transport for it."""
        srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        srv.bind((host, port))
        srv.listen(1)
        try:
            conn, _ = srv.accept()
        finally:
            srv.close()
        return cls(conn)

    def read(self, n=4096):
        try:
            data = self._sock.recv(n)
        except socket.timeout:
            return b""
        if data == b"":
            self.eof = True
        return data

    def write(self, data):
        self._sock.sendall(data)
        return len(data)

    def close(self):
        self._sock.close()
