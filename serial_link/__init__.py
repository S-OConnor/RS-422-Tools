"""serial_link — the RS-422/485 serial connection and on-wire protocol.

The core for both application domains in this repo: attitude streaming and
register command/control. Owns the cable, framing, codec, and link.

Layers:
  L0 transport  — the cable (Serial) + hardware-free stand-ins (Pty / Loopback)
  L1 framing    — RFC 1662 HDLC (flag, byte-stuffing, FCS-16)
  L2 codec      — struct-spec messages + type-id registry
  L3 link       — FramedLink: send/receive Message objects

Importing the package registers the built-in message catalog.
"""

# Each layer is its own sub-package; import them in dependency order (codec
# before link, since link depends on codec). The flat re-exports below give a
# convenience façade so callers can `from serial_link import FramedLink, ...`.
from . import transport, framing, codec, link  # noqa: F401

from .transport import Transport, SerialTransport, LoopbackTransport, PtyTransport
from .framing import FrameDecoder, Frame, encode
from .codec import (
    Field,
    Message,
    decode,
    registry,
    CodecError,
    UnknownMessage,
    AttitudeSample,
    ReadRegister,
    WriteRegister,
    ReadResponse,
    WriteAck,
    Nak,
    NAK_UNKNOWN_CMD,
    NAK_BAD_ADDR,
    NAK_BAD_LENGTH,
    NAK_READ_ONLY,
    NAK_REASONS,
)
from .link import FramedLink, ReceivedFrame

__all__ = [
    # L0 transport
    "Transport", "SerialTransport", "LoopbackTransport", "PtyTransport",
    # L1 framing
    "FrameDecoder", "Frame", "encode",
    # L2 codec
    "Field", "Message", "decode", "registry", "CodecError", "UnknownMessage",
    "AttitudeSample",
    "ReadRegister", "WriteRegister", "ReadResponse", "WriteAck", "Nak",
    "NAK_UNKNOWN_CMD", "NAK_BAD_ADDR", "NAK_BAD_LENGTH", "NAK_READ_ONLY",
    "NAK_REASONS",
    # L3 link
    "FramedLink", "ReceivedFrame",
]
