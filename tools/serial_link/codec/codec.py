"""L2 — struct-spec message codec.

A message is declared as a list of :class:`Field` (name + `struct` format char)
plus a 1-byte ``TYPE_ID``. ``__init_subclass__`` compiles a fixed
:class:`struct.Struct` and registers the type id, so the wire layout is explicit
and binary-exact — it maps 1:1 onto a C/C++ ``struct`` for the planned port.

An INFO field is ``type_id(1B) | body``; :func:`decode` reads the id and
dispatches to the right class.

**Fixed vs variable length.** Most messages are fixed-size (``FIELDS`` only). A
message may also declare a single trailing ``ARRAY`` field — a repeated element
packed after the fixed header. Its element count is implied by the frame length
(the HDLC layer delimits the body exactly), so no length prefix is needed. This
is what multi-register read responses use.
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
    They may optionally set ``ARRAY`` to a single :class:`Field` describing a
    trailing variable-length list of that element. ``ENDIAN`` defaults to
    big-endian / network order.
    """

    TYPE_ID = None
    FIELDS = ()
    ARRAY = None
    ENDIAN = ">"

    def __init_subclass__(cls, **kwargs):
        super().__init_subclass__(**kwargs)
        if cls.TYPE_ID is None:
            return
        cls._struct = struct.Struct(cls.ENDIAN + "".join(f.fmt for f in cls.FIELDS))
        cls._names = tuple(f.name for f in cls.FIELDS)
        cls._elem = struct.Struct(cls.ENDIAN + cls.ARRAY.fmt) if cls.ARRAY else None
        cls._array_name = cls.ARRAY.name if cls.ARRAY else None
        if cls.TYPE_ID in registry:
            raise CodecError(
                f"duplicate TYPE_ID 0x{cls.TYPE_ID:02X} "
                f"({cls.__name__} vs {registry[cls.TYPE_ID].__name__})"
            )
        registry[cls.TYPE_ID] = cls

    def __init__(self, **values):
        for name in self._names:
            setattr(self, name, values.get(name, 0))
        if self._array_name:
            setattr(self, self._array_name, list(values.get(self._array_name) or []))

    def pack_body(self):
        head = self._struct.pack(*(getattr(self, n) for n in self._names))
        if not self._elem:
            return head
        tail = b"".join(self._elem.pack(v) for v in getattr(self, self._array_name))
        return head + tail

    @classmethod
    def unpack_body(cls, body):
        head_size = cls._struct.size
        if not cls._elem:
            if len(body) != head_size:
                raise CodecError(
                    f"{cls.__name__}: body is {len(body)} bytes, expected {head_size}"
                )
            return cls(**dict(zip(cls._names, cls._struct.unpack(body))))

        if len(body) < head_size:
            raise CodecError(
                f"{cls.__name__}: body is {len(body)} bytes, "
                f"expected at least {head_size}"
            )
        rest = body[head_size:]
        esz = cls._elem.size
        if len(rest) % esz:
            raise CodecError(
                f"{cls.__name__}: trailing {len(rest)} bytes not a multiple "
                f"of element size {esz}"
            )
        values = dict(zip(cls._names, cls._struct.unpack(body[:head_size])))
        values[cls._array_name] = [
            cls._elem.unpack(rest[i:i + esz])[0] for i in range(0, len(rest), esz)
        ]
        return cls(**values)

    def encode(self):
        """Return the INFO field: ``type_id`` byte followed by the packed body."""
        return bytes((self.TYPE_ID,)) + self.pack_body()

    @classmethod
    def _all_names(cls):
        return cls._names + ((cls._array_name,) if cls._array_name else ())

    def __eq__(self, other):
        return (
            type(self) is type(other)
            and all(getattr(self, n) == getattr(other, n) for n in self._all_names())
        )

    def __repr__(self):
        body = ", ".join(f"{n}={getattr(self, n)!r}" for n in self._all_names())
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
