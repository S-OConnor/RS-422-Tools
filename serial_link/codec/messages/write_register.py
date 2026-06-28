"""Register write request (one message per file)."""

from ..codec import Field, Message


class WriteRegister(Message):
    """Host -> device: write ``value`` to register ``addr``."""

    TYPE_ID = 0x02
    FIELDS = (
        Field("addr", "H"),     # u16: register address
        Field("value", "H"),    # u16: value to write
    )
