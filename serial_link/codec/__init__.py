"""L2 — Codec: struct-spec messages and the type-id registry.

Importing this package registers the built-in message catalog (``messages``).
"""

from .codec import Field, Message, decode, registry, CodecError, UnknownMessage
from . import messages  # noqa: F401  (registers the catalog)
from .messages import (
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

__all__ = [
    "Field", "Message", "decode", "registry", "CodecError", "UnknownMessage",
    "AttitudeSample",
    "ReadRegister", "WriteRegister", "ReadResponse", "WriteAck", "Nak",
    "NAK_UNKNOWN_CMD", "NAK_BAD_ADDR", "NAK_BAD_LENGTH", "NAK_READ_ONLY",
    "NAK_REASONS",
]
