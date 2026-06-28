"""Negative acknowledgement (one message per file).

Type-id 0xFF: error reply to any request the device could not satisfy.
"""

from ..codec import Field, Message

# Nak reason codes.
NAK_UNKNOWN_CMD = 1   # message type not understood
NAK_BAD_ADDR = 2      # address (or address+count) out of range
NAK_BAD_LENGTH = 3    # malformed request body
NAK_READ_ONLY = 4     # write to a read-only register

NAK_REASONS = {
    NAK_UNKNOWN_CMD: "unknown command",
    NAK_BAD_ADDR: "bad address",
    NAK_BAD_LENGTH: "bad length",
    NAK_READ_ONLY: "read-only",
}


class Nak(Message):
    """Device -> host: a request was rejected; ``code`` says why."""

    TYPE_ID = 0xFF
    FIELDS = (
        Field("code", "B"),     # u8:  one of NAK_*
        Field("addr", "H"),     # u16: offending address (0 if N/A)
    )

    def reason(self):
        return NAK_REASONS.get(self.code, f"code {self.code}")
