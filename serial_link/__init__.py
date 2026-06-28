"""serial_link — the RS-422/485 serial connection to the attitude sensor.

This package owns the cable and the on-wire protocol.

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
    # L3 link
    "FramedLink", "ReceivedFrame",
]
