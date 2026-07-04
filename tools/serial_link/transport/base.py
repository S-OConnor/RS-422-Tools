"""L0 — the Transport seam.

``Transport`` is the one interface everything above it (framing, codec, link,
apps) is written against. Nothing above this layer knows or cares whether the
bytes come from the real RS-422 cable or a hardware-free stand-in.

``read`` returns whatever bytes are available (possibly ``b""`` on timeout) and
never blocks forever. ``eof`` reports that the far end has closed, so a receive
loop can terminate instead of spinning.
"""

from abc import ABC, abstractmethod


class Transport(ABC):
    """Bidirectional byte pipe."""

    #: True once the peer has closed / the device is gone.
    eof = False

    @abstractmethod
    def read(self, n=4096):
        """Return up to ``n`` bytes; ``b""`` if none are available right now."""

    @abstractmethod
    def write(self, data):
        """Write all of ``data``; return the number of bytes written."""

    def close(self):
        pass

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()
