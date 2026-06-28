"""L1 — Framing: RFC 1662 HDLC framing and its FCS-16 frame check."""

from .fcs import INIT_FCS16, GOOD_FCS16, FCSTAB, fcs16, frame_fcs, check_fcs
from .framing import (
    FLAG,
    ESC,
    XOR,
    DEFAULT_ADDRESS,
    DEFAULT_CONTROL,
    Frame,
    encode,
    FrameDecoder,
)

__all__ = [
    "INIT_FCS16", "GOOD_FCS16", "FCSTAB", "fcs16", "frame_fcs", "check_fcs",
    "FLAG", "ESC", "XOR", "DEFAULT_ADDRESS", "DEFAULT_CONTROL",
    "Frame", "encode", "FrameDecoder",
]
