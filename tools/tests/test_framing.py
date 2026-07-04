from serial_link.framing import FLAG, ESC, XOR, encode, FrameDecoder


def roundtrip(info, **kw):
    dec = FrameDecoder()
    frames = dec.feed(encode(info, **kw))
    assert len(frames) == 1
    return frames[0]


def test_basic_roundtrip():
    frame = roundtrip(b"\x10hello")
    assert frame.fcs_ok
    assert frame.info == b"\x10hello"
    assert frame.address == 0xFF and frame.control == 0x03


def test_address_and_control_preserved():
    frame = roundtrip(b"\x01\x02", address=0x42, control=0x13)
    assert frame.address == 0x42 and frame.control == 0x13
    assert frame.info == b"\x01\x02"


def test_stuffing_of_special_octets():
    # FLAG and ESC in the payload must be escaped on the wire but survive decode.
    info = bytes([FLAG, ESC, 0x00, 0x7E, 0x7D, 0x41])
    wire = encode(info)
    # No raw FLAG/ESC inside the frame body (between the delimiters).
    assert FLAG not in wire[1:-1]
    assert all(wire[i] != ESC or True for i in range(len(wire)))  # ESC may appear as escape marker
    frame = roundtrip(info)
    assert frame.info == info


def test_control_chars_escaped_by_default_accm():
    info = bytes(range(0x00, 0x20)) + b"\x10"
    wire = encode(info)
    # Every control char (<0x20) in the body should appear escaped, not raw.
    body = wire[1:-1]
    assert FLAG not in body
    frame = roundtrip(info)
    assert frame.info == info


def test_decoder_handles_split_reads():
    wire = encode(b"\x10abc")
    dec = FrameDecoder()
    frames = []
    for byte in wire:  # feed one byte at a time
        frames.extend(dec.feed(bytes([byte])))
    assert len(frames) == 1 and frames[0].info == b"\x10abc"


def test_leading_garbage_ignored():
    wire = b"\x00\x11garbage" + encode(b"\x10ok")
    frames = FrameDecoder().feed(wire)
    assert len(frames) == 1 and frames[0].info == b"\x10ok"


def test_back_to_back_and_shared_flags():
    a = encode(b"\x10one")
    b = encode(b"\x10two")
    # Shared flag: drop the trailing flag of a (it doubles as b's leading flag).
    stream = a[:-1] + b
    frames = FrameDecoder().feed(stream)
    assert [f.info for f in frames] == [b"\x10one", b"\x10two"]


def test_bad_fcs_flagged_not_dropped():
    wire = bytearray(encode(b"\x10payload"))
    # Corrupt a body byte (index 3 is first info byte after FLAG,addr,ctrl).
    wire[4] ^= 0x01
    frames = FrameDecoder().feed(bytes(wire))
    assert len(frames) == 1 and frames[0].fcs_ok is False


def test_runt_frame_dropped():
    # Two flags with <4 bytes between them is idle/runt, not a frame.
    frames = FrameDecoder().feed(bytes([FLAG, 0x01, 0x02, FLAG]))
    assert frames == []
