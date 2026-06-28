"""L2 — struct-spec message codec.

A message is declared as a list of :class:`Field` (name + `struct` format char)
plus a 1-byte ``TYPE_ID``. The metaclass machinery (via ``__init_subclass__``)
compiles a fixed :class:`struct.Struct` and registers the type id, so the wire
layout is explicit and binary-exact — it maps 1:1 onto a C/C++ ``struct`` for the
planned port.

An INFO field is ``type_id(1B) | body``; :func:`decode` reads the id and
dispatches to the right class. Fixed-size messages only for now; variable-length
payloads (e.g. multi-register reads) are a Phase 2 extension.
"""

import struct


class CodecError(Exception):
    """Base class for codec errors."""


class UnknownMessage(CodecError):
    """Raised when an INFO field carries an unregistered type id."""

    def __init__(self, type_id, info):
        super().__init__(f"unknown message type id 0x{type_id:02X}")
        self.type_id = type_id
        self.info = info


class Field:
    """One field of a message: a name and a `struct` format code (e.g. 'H', 'i')."""

    __slots__ = ("name", "fmt")

    def __init__(self, name, fmt):
        self.name = name
        self.fmt = fmt


# type_id -> Message subclass
registry = {}


class Message:
    """Base class for struct-spec messages.

    Subclasses set ``TYPE_ID`` (int) and ``FIELDS`` (sequence of :class:`Field`).
    ``ENDIAN`` defaults to big-endian / network order.
    """

    TYPE_ID = None
    FIELDS = ()
    ENDIAN = ">"

    def __init_subclass__(cls, **kwargs):
        super().__init_subclass__(**kwargs)
        if cls.TYPE_ID is None:
            return
        cls._struct = struct.Struct(cls.ENDIAN + "".join(f.fmt for f in cls.FIELDS))
        cls._names = tuple(f.name for f in cls.FIELDS)
        if cls.TYPE_ID in registry:
            raise CodecError(
                f"duplicate TYPE_ID 0x{cls.TYPE_ID:02X} "
                f"({cls.__name__} vs {registry[cls.TYPE_ID].__name__})"
            )
        registry[cls.TYPE_ID] = cls

    def __init__(self, **values):
        for name in self._names:
            setattr(self, name, values.get(name, 0))

    def pack_body(self):
        return self._struct.pack(*(getattr(self, n) for n in self._names))

    @classmethod
    def unpack_body(cls, body):
        if len(body) != cls._struct.size:
            raise CodecError(
                f"{cls.__name__}: body is {len(body)} bytes, "
                f"expected {cls._struct.size}"
            )
        values = cls._struct.unpack(body)
        return cls(**dict(zip(cls._names, values)))

    def encode(self):
        """Return the INFO field: ``type_id`` byte followed by the packed body."""
        return bytes((self.TYPE_ID,)) + self.pack_body()

    def __eq__(self, other):
        return (
            type(self) is type(other)
            and all(getattr(self, n) == getattr(other, n) for n in self._names)
        )

    def __repr__(self):
        body = ", ".join(f"{n}={getattr(self, n)!r}" for n in self._names)
        return f"{type(self).__name__}({body})"


def decode(info):
    """Decode an INFO field (``type_id`` + body) into a :class:`Message`."""
    if not info:
        raise CodecError("empty INFO field")
    type_id = info[0]
    cls = registry.get(type_id)
    if cls is None:
        raise UnknownMessage(type_id, bytes(info))
    return cls.unpack_body(info[1:])
