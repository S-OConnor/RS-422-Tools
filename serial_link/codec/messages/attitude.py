"""Attitude telemetry (one message per file).

Type-id range 0x20-0x2F: attitude / motion telemetry.
"""

from ..codec import Field, Message


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
