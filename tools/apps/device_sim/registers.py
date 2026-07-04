"""Register model + request handler for the device simulator.

`RegisterStore` is the device's state (a flat array of 16-bit registers).
`respond` maps an incoming request message to the reply the device should send
back — the device's whole "brain", kept pure so it is easy to test.
"""

from serial_link import (
    ReadRegister, WriteRegister, ReadResponse, WriteAck, Nak,
    NAK_UNKNOWN_CMD, NAK_BAD_ADDR, NAK_READ_ONLY,
)

U16 = 0xFFFF


class RegisterStore:
    """A flat bank of ``size`` 16-bit registers, addressed 0..size-1."""

    def __init__(self, size=256, seed=None, readonly=()):
        self.size = size
        self._regs = [0] * size
        self._readonly = set(readonly)
        if seed:
            for addr, value in seed.items():
                self._regs[addr] = value & U16

    def read(self, addr, count=1):
        if addr < 0 or count < 1 or addr + count > self.size:
            raise IndexError(f"read {addr}..{addr + count - 1} out of range")
        return self._regs[addr:addr + count]

    def write(self, addr, value):
        if addr < 0 or addr >= self.size:
            raise IndexError(f"write {addr} out of range")
        if addr in self._readonly:
            raise PermissionError(f"register {addr} is read-only")
        self._regs[addr] = value & U16
        return self._regs[addr]


def respond(store, message):
    """Map a request ``message`` to the reply :class:`Message`, or ``None``.

    Returns a ReadResponse / WriteAck on success, a Nak on a bad request, or
    ``None`` if the message is not a request the device answers (e.g. a stray
    response/telemetry frame — nothing to reply to).
    """
    if isinstance(message, ReadRegister):
        try:
            values = store.read(message.addr, message.count)
        except IndexError:
            return Nak(code=NAK_BAD_ADDR, addr=message.addr)
        return ReadResponse(addr=message.addr, values=values)

    if isinstance(message, WriteRegister):
        try:
            stored = store.write(message.addr, message.value)
        except IndexError:
            return Nak(code=NAK_BAD_ADDR, addr=message.addr)
        except PermissionError:
            return Nak(code=NAK_READ_ONLY, addr=message.addr)
        return WriteAck(addr=message.addr, value=stored, status=0)

    # A known message that isn't a request we serve (e.g. a response or telemetry).
    if isinstance(message, (ReadResponse, WriteAck, Nak)):
        return None

    return Nak(code=NAK_UNKNOWN_CMD, addr=0)
