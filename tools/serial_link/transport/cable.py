"""The real RS-422/485 cable — the only transport that touches hardware."""

from .base import Transport


class SerialTransport(Transport):
    """The Adafruit FT231X cable (8N1).

    pyserial is imported lazily so the rest of the SDK (and the virtual
    transports) stay usable without it installed.
    """

    def __init__(self, port, baud=115200, timeout=0.1):
        import serial  # lazy: only needed for real hardware

        self._ser = serial.Serial(
            port=port,
            baudrate=baud,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=timeout,
        )

    def read(self, n=4096):
        # in_waiting avoids blocking for the full count; timeout bounds the wait.
        want = self._ser.in_waiting or 1
        return self._ser.read(min(n, want))

    def write(self, data):
        return self._ser.write(data)

    def close(self):
        self._ser.close()
