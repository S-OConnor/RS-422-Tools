import pytest

from serial_link import (
    ReadRegister, WriteRegister, ReadResponse, WriteAck, Nak,
    AttitudeSample, NAK_BAD_ADDR, NAK_READ_ONLY, NAK_UNKNOWN_CMD,
)
from apps.device_sim.registers import RegisterStore, respond


def test_store_read_write():
    s = RegisterStore(size=16)
    assert s.read(0, 4) == [0, 0, 0, 0]
    s.write(2, 0x1234)
    assert s.read(2) == [0x1234]


def test_store_seed_and_readonly():
    s = RegisterStore(size=16, seed={1: 0xAAAA}, readonly={1})
    assert s.read(1) == [0xAAAA]
    with pytest.raises(PermissionError):
        s.write(1, 0)


def test_store_out_of_range():
    s = RegisterStore(size=4)
    with pytest.raises(IndexError):
        s.read(3, 2)
    with pytest.raises(IndexError):
        s.write(4, 0)


def test_respond_read_returns_values():
    s = RegisterStore(size=8, seed={0: 10, 1: 20, 2: 30})
    reply = respond(s, ReadRegister(addr=0, count=3))
    assert isinstance(reply, ReadResponse)
    assert reply.values == [10, 20, 30] and reply.addr == 0


def test_respond_write_acks_and_persists():
    s = RegisterStore(size=8)
    reply = respond(s, WriteRegister(addr=5, value=0x00FF))
    assert isinstance(reply, WriteAck) and reply.value == 0x00FF and reply.status == 0
    assert s.read(5) == [0x00FF]


def test_respond_bad_addr_nak():
    s = RegisterStore(size=4)
    reply = respond(s, ReadRegister(addr=3, count=5))
    assert isinstance(reply, Nak) and reply.code == NAK_BAD_ADDR


def test_respond_readonly_nak():
    s = RegisterStore(size=8, readonly={2})
    reply = respond(s, WriteRegister(addr=2, value=1))
    assert isinstance(reply, Nak) and reply.code == NAK_READ_ONLY


def test_respond_unknown_command_nak():
    s = RegisterStore(size=8)
    reply = respond(s, AttitudeSample(seq=1, t_ms=0, roll=0, pitch=0, yaw=0))
    assert isinstance(reply, Nak) and reply.code == NAK_UNKNOWN_CMD


def test_respond_ignores_responses():
    s = RegisterStore(size=8)
    assert respond(s, WriteAck(addr=0, value=0, status=0)) is None
