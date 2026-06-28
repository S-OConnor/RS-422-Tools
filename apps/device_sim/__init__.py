"""device_sim app — register device stand-in (RX+TX, C2 slave)."""

from .device_sim import main
from .registers import RegisterStore, respond

__all__ = ["main", "RegisterStore", "respond"]
