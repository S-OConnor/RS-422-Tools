"""Message catalog — one module per message type.

Importing this package imports every message module, which registers each
``TYPE_ID`` with the codec registry. Add a new message by dropping a new module
here and importing it below.

Type-id ranges:
  0x01-0x0F  register command/control requests
  0x20-0x2F  attitude / motion telemetry
  0x80-0x8F  register responses
  0xFF       Nak (error reply)
"""

from .attitude import AttitudeSample
from .read_register import ReadRegister
from .write_register import WriteRegister
from .read_response import ReadResponse
from .write_ack import WriteAck
from .nak import (
    Nak,
    NAK_UNKNOWN_CMD,
    NAK_BAD_ADDR,
    NAK_BAD_LENGTH,
    NAK_READ_ONLY,
    NAK_REASONS,
)

__all__ = [
    "AttitudeSample",
    "ReadRegister",
    "WriteRegister",
    "ReadResponse",
    "WriteAck",
    "Nak",
    "NAK_UNKNOWN_CMD",
    "NAK_BAD_ADDR",
    "NAK_BAD_LENGTH",
    "NAK_READ_ONLY",
    "NAK_REASONS",
]
