"""L2 — concrete message catalog.

Importing this module registers every message type with :mod:`serial_link.codec`.
For the attitude-sensor link there is exactly one message: a periodic Euler
attitude sample streamed at 10 Hz.

Type-id ranges (convention):
  0x20-0x2F  attitude / motion telemetry
"""

from .codec import Field, Message


class AttitudeSample(Message):
    """Device -> host: one Euler attitude sample (streamed periodically).

    Fixed 20-byte body, big-endian; maps 1:1 to a packed C/C++ struct.
    """

    TYPE_ID = 0x20
    FIELDS = (
        Field("seq", "I"),      # u32: increments per sample → drop/gap detection
        Field("t_ms", "I"),     # u32: sensor timestamp, ms since start
        Field("roll", "f"),     # f32: degrees
        Field("pitch", "f"),    # f32: degrees
        Field("yaw", "f"),      # f32: degrees
    )
