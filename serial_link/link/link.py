"""L3 — FramedLink: ties a Transport + framing + codec together.

This is the opt-in convenience layer. It forces no concurrency model: ``receive``
is a plain blocking generator and ``request`` is a blocking send-then-await, so an
app can drive either from the main thread, a worker thread, or wrap it however it
likes.
"""

import time
from collections import deque

from .. import codec
from ..framing import FrameDecoder, encode, DEFAULT_ADDRESS, DEFAULT_CONTROL


class ReceivedFrame:
    """A frame as it came off the link, with decode lazily attempted.

    ``message`` is the decoded :class:`~serial_link.codec.Message`, or ``None`` if the
    FCS failed or the type id was unknown — ``error`` then says why. Either way
    the raw ``frame`` is available for logging.
    """

    __slots__ = ("frame", "message", "error")

    def __init__(self, frame, message=None, error=None):
        self.frame = frame
        self.message = message
        self.error = error

    @property
    def ok(self):
        return self.message is not None

    def __repr__(self):
        if self.ok:
            return f"ReceivedFrame(addr=0x{self.frame.address:02X}, {self.message!r})"
        return f"ReceivedFrame(addr=0x{self.frame.address:02X}, error={self.error!r})"


class FramedLink:
    """Send/receive :class:`Message` objects over a :class:`Transport`."""

    def __init__(self, transport, read_size=4096):
        self._transport = transport
        self._decoder = FrameDecoder()
        self._read_size = read_size
        self._inbox = deque()

    def send(self, message, address=DEFAULT_ADDRESS, control=DEFAULT_CONTROL):
        """Frame and transmit one message."""
        self._transport.write(encode(message.encode(), address, control))

    def _next_frame(self, timeout=None):
        """Return one :class:`ReceivedFrame`, or ``None`` on EOF/timeout.

        ``timeout`` of ``None`` blocks until a frame arrives or the transport
        reaches EOF. Already-decoded frames are buffered so nothing is lost
        between :meth:`receive` and :meth:`request`.
        """
        if self._inbox:
            return self._inbox.popleft()
        deadline = None if timeout is None else time.monotonic() + timeout
        while True:
            data = self._transport.read(self._read_size)
            if data:
                for frame in self._decoder.feed(data):
                    self._inbox.append(self._wrap(frame))
                if self._inbox:
                    return self._inbox.popleft()
            elif self._transport.eof:
                return None
            if deadline is not None and time.monotonic() >= deadline:
                return None

    def receive(self):
        """Yield :class:`ReceivedFrame` objects until the transport reaches EOF.

        Blocks waiting for bytes between frames. Bad-FCS and unknown-type frames
        are yielded (with ``ok == False``) rather than dropped, so callers decide
        how to handle them.
        """
        while True:
            rf = self._next_frame(None)
            if rf is None:
                return
            yield rf

    def request(self, message, timeout=1.0, address=DEFAULT_ADDRESS,
                control=DEFAULT_CONTROL):
        """Send ``message`` and return the next received frame (the reply).

        Returns a :class:`ReceivedFrame`, or ``None`` if no frame arrived within
        ``timeout`` seconds. Correlation is implicit (the next frame is the reply),
        which holds on a point-to-point / strict request-response bus.
        """
        self.send(message, address, control)
        return self._next_frame(timeout)

    @staticmethod
    def _wrap(frame):
        if not frame.fcs_ok:
            return ReceivedFrame(frame, error="bad FCS")
        try:
            return ReceivedFrame(frame, message=codec.decode(frame.info))
        except codec.CodecError as exc:
            return ReceivedFrame(frame, error=str(exc))

    def close(self):
        self._transport.close()
