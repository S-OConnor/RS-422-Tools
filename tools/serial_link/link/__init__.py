"""L3 — Link: FramedLink ties Transport + framing + codec together."""

from .link import FramedLink, ReceivedFrame

__all__ = ["FramedLink", "ReceivedFrame"]
