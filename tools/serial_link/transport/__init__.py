"""L0 — Transport: the raw byte pipe.

  base.py     the Transport seam (interface)
  cable.py    SerialTransport      — the real RS-422 cable (hardware)
  virtual.py  PtyTransport,        — stand-ins for the cable (no hardware);
              LoopbackTransport      used by every test and demo
"""

from .base import Transport
from .cable import SerialTransport
from .virtual import PtyTransport, LoopbackTransport

__all__ = ["Transport", "SerialTransport", "PtyTransport", "LoopbackTransport"]
