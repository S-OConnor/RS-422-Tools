// L2 — codec dispatch: INFO field <-> typed Message.
//
// A decoded message is a std::variant over the catalog. decode() reads the
// leading type-id byte and dispatches to the matching struct's unpack_body();
// std::visit / std::get_if let callers switch on the concrete type. This is the
// C++ analogue of Python's `type_id -> Message subclass` registry.
#pragma once

#include <string>
#include <variant>

#include "serial_link/common.hpp"
#include "serial_link/messages.hpp"

namespace serial_link::codec {

// The set of messages that can appear on the wire.
using Message = std::variant<AttitudeSample, ReadRegister, WriteRegister,
                             ReadResponse, WriteAck, Nak>;

// Decode an INFO field (type_id + body) into a Message.
// Throws UnknownMessage for an unregistered type id, or CodecError for a
// malformed/empty body.
Message decode(const Bytes& info);

// The type-id byte for whatever message the variant currently holds.
std::uint8_t type_id_of(const Message& msg);

// Encode a Message variant to its INFO field (type_id + body).
Bytes encode_message(const Message& msg);

// A human-readable one-line rendering (used for logging).
std::string to_string(const Message& msg);

}  // namespace serial_link::codec
