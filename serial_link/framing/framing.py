"""L1 — RFC 1662 HDLC-like framing.

A frame on the wire looks like::

    FLAG | address | control | INFO | FCS-16 | FLAG

``FLAG`` (0x7E) delimits frames. Within a frame, octet transparency is applied:
any ``FLAG``, ``ESC`` (0x7D), or octet flagged in the ACCM is sent as
``ESC, octet ^ 0x20``. The FCS-16 covers address+control+INFO (see :mod:`serial_link.fcs`).

This module is transport-agnostic: :func:`encode` turns an INFO field into wire
bytes, and :class:`FrameDecoder` turns an arbitrary byte stream back into frames.
"""

from collections import namedtuple

from .fcs import frame_fcs, check_fcs

FLAG = 0x7E
ESC = 0x7D
XOR = 0x20

# PPP defaults: All-Stations address + Unnumbered Information control.
DEFAULT_ADDRESS = 0xFF
DEFAULT_CONTROL = 0x03

# A decoded frame. ``fcs_ok`` lets callers see (and log) corrupt frames rather
# than silently dropping them.
Frame = namedtuple("Frame", ["address", "control", "info", "fcs_ok"])


def _accm_hit(byte, accm):
    """True if ``byte`` (< 0x20) must be escaped per the async control char map.

    ``accm`` is a 32-bit mask; bit N set means "escape control char N". The
    default (``None``) escapes all control characters, the conservative choice.
    """
    if byte >= 0x20:
        return False
    if accm is None:
        return True
    return bool(accm & (1 << byte))


def encode(info, address=DEFAULT_ADDRESS, control=DEFAULT_CONTROL, accm=None):
    """Frame an INFO field into wire bytes (with delimiting flags and FCS)."""
    payload = bytes((address, control)) + bytes(info)
    payload += frame_fcs(payload)

    out = bytearray((FLAG,))
    for byte in payload:
        if byte in (FLAG, ESC) or _accm_hit(byte, accm):
            out.append(ESC)
            out.append(byte ^ XOR)
        else:
            out.append(byte)
    out.append(FLAG)
    return bytes(out)


class FrameDecoder:
    """Streaming de-framer. Feed it arbitrary byte chunks; get back frames.

    Tolerates split reads, leading garbage before the first flag, and shared or
    back-to-back flags between frames. Frames shorter than the minimum
    (address+control+FCS) are treated as runts/idle and dropped.
    """

    _MIN_LEN = 4  # address + control + 2 FCS bytes

    def __init__(self):
        self._buf = bytearray()
        self._in_frame = False
        self._esc = False

    def feed(self, data):
        """Consume ``data``; return a list of complete :class:`Frame` objects."""
        frames = []
        for byte in data:
            if byte == FLAG:
                if self._in_frame and self._buf:
                    frame = self._finish()
                    if frame is not None:
                        frames.append(frame)
                self._buf.clear()
                self._in_frame = True
                self._esc = False
            elif not self._in_frame:
                continue  # garbage before the first flag
            elif byte == ESC:
                self._esc = True
            elif self._esc:
                self._buf.append(byte ^ XOR)
                self._esc = False
            else:
                self._buf.append(byte)
        return frames

    def _finish(self):
        raw = bytes(self._buf)
        if len(raw) < self._MIN_LEN:
            return None
        return Frame(
            address=raw[0],
            control=raw[1],
            info=raw[2:-2],
            fcs_ok=check_fcs(raw),
        )
