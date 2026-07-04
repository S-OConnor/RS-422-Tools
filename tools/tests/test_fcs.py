from serial_link.framing import fcs


def test_table_has_256_entries_in_range():
    assert len(fcs.FCSTAB) == 256
    assert all(0 <= v <= 0xFFFF for v in fcs.FCSTAB)


def test_good_fcs_residual():
    # Running the FCS over payload + appended FCS yields the magic constant.
    for payload in (b"", b"\x00", b"\xff\x03\x01\x02\x03", bytes(range(64))):
        framed = payload + fcs.frame_fcs(payload)
        assert fcs.fcs16(framed) == fcs.GOOD_FCS16
        assert fcs.check_fcs(framed)


def test_check_fcs_rejects_corruption():
    payload = b"\xff\x03hello"
    framed = bytearray(payload + fcs.frame_fcs(payload))
    framed[3] ^= 0x01  # flip a bit
    assert not fcs.check_fcs(bytes(framed))


def test_frame_fcs_shape_and_determinism():
    out = fcs.frame_fcs(b"\xff\x03\x21")
    assert isinstance(out, bytes) and len(out) == 2
    assert out == fcs.frame_fcs(b"\xff\x03\x21")  # deterministic
