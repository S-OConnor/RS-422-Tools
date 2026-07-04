"""Register read response — variable-length (one message per file).

Type-id range 0x80-0x8F: register responses.
"""

from ..codec import Field, Message


class ReadResponse(Message):
    """Device -> host: the register values for a preceding ReadRegister.

    ``values`` is a variable-length list of u16; its length is the requested
    count (implied by the frame size).
    """

    TYPE_ID = 0x81
    FIELDS = (Field("addr", "H"),)      # u16: first register address (echo)
    ARRAY = Field("values", "H")        # u16[]: the register values
