"""RFC 1662 FCS-16 (HDLC/PPP frame check sequence).

The 16-bit FCS uses the CRC-CCITT polynomial x^16 + x^12 + x^5 + 1 in its
*reflected* form (0x8408), exactly as specified in RFC 1662 Appendix C. The
256-entry lookup table there is reproduced here by generation rather than by
transcription, which removes any chance of a copy error.

Wire conventions (RFC 1662):
  * the running FCS is seeded with 0xFFFF;
  * the value transmitted is the *ones-complement* of the running FCS, sent
    low byte first (see :func:`frame_fcs`);
  * a receiver that runs :func:`fcs16` over ``info + fcs`` gets the constant
    "good FCS" residual 0xF0B8 when the frame is intact (see :func:`check_fcs`).
"""

INIT_FCS16 = 0xFFFF
GOOD_FCS16 = 0xF0B8


def _build_table():
    table = []
    for byte in range(256):
        crc = byte
        for _ in range(8):
            crc = (crc >> 1) ^ 0x8408 if crc & 1 else crc >> 1
        table.append(crc & 0xFFFF)
    return tuple(table)


FCSTAB = _build_table()


def fcs16(data, fcs=INIT_FCS16):
    """Return the running FCS-16 over ``data`` (not complemented).

    ``fcs`` may be passed to continue a computation across chunks.
    """
    for byte in data:
        fcs = (fcs >> 8) ^ FCSTAB[(fcs ^ byte) & 0xFF]
    return fcs


def frame_fcs(data):
    """Return the 2 FCS bytes to append after ``data``, low byte first."""
    fcs = fcs16(data) ^ 0xFFFF
    return bytes((fcs & 0xFF, (fcs >> 8) & 0xFF))


def check_fcs(data_with_fcs):
    """True if ``data_with_fcs`` (payload followed by its 2 FCS bytes) is intact."""
    return fcs16(data_with_fcs) == GOOD_FCS16
