"""Register read request (one message per file).

Type-id range 0x01-0x0F: register command/control requests.
"""

from ..codec import Field, Message


class ReadRegister(Message):
    """Host -> device: read ``count`` registers starting at ``addr``."""

    TYPE_ID = 0x01
    FIELDS = (
        Field("addr", "H"),     # u16: first register address
        Field("count", "B"),    # u8:  number of registers to read
    )
