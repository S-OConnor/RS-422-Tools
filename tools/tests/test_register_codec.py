import pytest

from serial_link import (
    decode, codec,
    ReadRegister, WriteRegister, ReadResponse, WriteAck, Nak,
)


@pytest.mark.parametrize("msg", [
    ReadRegister(addr=0x0010, count=4),
    WriteRegister(addr=0x0020, value=0x1234),
    WriteAck(addr=0x0020, value=0x1234, status=0),
    Nak(code=2, addr=0x0020),
    ReadResponse(addr=0x10, values=[1, 2, 3]),
    ReadResponse(addr=0x10, values=[0xBEEF]),
    ReadResponse(addr=0x10, values=[]),          # empty array
])
def test_register_messages_roundtrip(msg):
    assert decode(msg.encode()) == msg


def test_type_ids_registered():
    for cls, tid in [(ReadRegister, 0x01), (WriteRegister, 0x02),
                     (ReadResponse, 0x81), (WriteAck, 0x82), (Nak, 0xFF)]:
        assert codec.registry[tid] is cls


def test_read_response_variable_length_size():
    info = ReadResponse(addr=0x10, values=[0xAAAA, 0xBBBB, 0xCCCC]).encode()
    # type_id(1) + addr(2) + 3 * u16(2) = 9
    assert len(info) == 1 + 2 + 3 * 2


def test_read_response_bad_trailing_length_raises():
    # addr(2) + an odd trailing byte -> not a whole number of u16 elements
    bad = bytes([ReadResponse.TYPE_ID, 0x00, 0x10, 0xAA])
    with pytest.raises(codec.CodecError):
        decode(bad)


def test_nak_reason_text():
    assert Nak(code=2, addr=0).reason() == "bad address"
