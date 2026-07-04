"""sensor_sim app — synthesise an attitude stream @ 10 Hz (TX, test stand-in)."""

from .sensor_sim import main, stream, attitude_at

__all__ = ["main", "stream", "attitude_at"]
