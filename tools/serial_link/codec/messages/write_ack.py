"""Register write acknowledgement (one message per file)."""

from ..codec import Field, Message


class WriteAck(Message):
    """Device -> host: confirms a WriteRegister; echoes the stored value."""

    TYPE_ID = 0x82
    FIELDS = (
        Field("addr", "H"),     # u16: register address (echo)
        Field("value", "H"),    # u16: value now stored
        Field("status", "B"),   # u8:  0 = OK
    )
