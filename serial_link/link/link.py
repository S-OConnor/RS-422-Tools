"""L3 — FramedLink: ties a Transport + framing + codec together.

This is the opt-in convenience layer. It forces no concurrency model: ``receive``
is a plain blocking generator, so an app can drive it from the main thread, a
worker thread, or wrap it however it likes.
"""

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

    def send(self, message, address=DEFAULT_ADDRESS, control=DEFAULT_CONTROL):
        """Frame and transmit one message."""
        self._transport.write(encode(message.encode(), address, control))

    def receive(self):
        """Yield :class:`ReceivedFrame` objects until the transport reaches EOF.

        Blocks waiting for bytes between frames. Bad-FCS and unknown-type frames
        are yielded (with ``ok == False``) rather than dropped, so callers decide
        how to handle them.
        """
        while True:
            data = self._transport.read(self._read_size)
            if data:
                for frame in self._decoder.feed(data):
                    yield self._wrap(frame)
            elif self._transport.eof:
                return

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
