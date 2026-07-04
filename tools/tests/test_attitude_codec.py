import struct

import pytest

from serial_link import decode, AttitudeSample
from serial_link.codec import registry


def test_type_id_registered():
    assert registry[0x20] is AttitudeSample
    assert AttitudeSample.TYPE_ID == 0x20


def test_body_is_20_bytes():
    info = AttitudeSample(seq=1, t_ms=2, roll=0, pitch=0, yaw=0).encode()
    assert len(info) == 21          # 1 type-id + 20 body
    assert info[0] == 0x20


def test_roundtrip_floats():
    msg = AttitudeSample(seq=1234, t_ms=56789, roll=12.5, pitch=-3.25, yaw=180.0)
    back = decode(msg.encode())
    assert back.seq == 1234 and back.t_ms == 56789
    # 12.5 / -3.25 / 180.0 are exactly representable in float32
    assert (back.roll, back.pitch, back.yaw) == (12.5, -3.25, 180.0)


def test_roundtrip_inexact_floats_within_f32():
    msg = AttitudeSample(seq=0, t_ms=0, roll=15.0, pitch=-7.123, yaw=359.9)
    back = decode(msg.encode())
    assert back.pitch == pytest.approx(-7.123, abs=1e-3)
    assert back.yaw == pytest.approx(359.9, abs=1e-3)


def test_big_endian_wire_layout():
    # type_id + >IIfff matches the documented layout exactly.
    msg = AttitudeSample(seq=1, t_ms=2, roll=1.0, pitch=2.0, yaw=3.0)
    expected = bytes([0x20]) + struct.pack(">IIfff", 1, 2, 1.0, 2.0, 3.0)
    assert msg.encode() == expected
