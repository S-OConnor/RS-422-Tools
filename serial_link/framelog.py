"""Shared logging helpers: render frames/bytes for humans."""


def hexdump(data):
    """Space-separated hex, e.g. ``7E FF 03 10 ...``."""
    return " ".join(f"{b:02X}" for b in data)


def format_received(received):
    """One-line summary of a :class:`~serial_link.link.ReceivedFrame`."""
    addr = received.frame.address
    info_hex = hexdump(received.frame.info)
    if received.ok:
        return f"addr=0x{addr:02X}  {received.message!r}  [{info_hex}]"
    return f"addr=0x{addr:02X}  !! {received.error}  [{info_hex}]"
