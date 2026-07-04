// L2 — the message catalog.
//
// Each message is a plain struct whose fields map 1:1 onto the big-endian wire
// body — the whole point of the struct-spec codec in the Python original, made
// literal here. Every message provides:
//   * static constexpr TYPE_ID
//   * encode()      -> INFO field (type_id byte + packed big-endian body)
//   * unpack_body() -> parse a body (no type_id) back into the struct
//
// Type-id ranges:
//   0x01-0x0F  register command/control requests
//   0x20-0x2F  attitude / motion telemetry
//   0x80-0x8F  register responses
//   0xFF       Nak (error reply)
#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "serial_link/common.hpp"

namespace serial_link::codec {

// Base class for codec errors (mirrors Python CodecError).
class CodecError : public std::runtime_error {
public:
    explicit CodecError(const std::string& what) : std::runtime_error(what) {}
};

// Raised when an INFO field carries an unregistered type id.
class UnknownMessage : public CodecError {
public:
    UnknownMessage(std::uint8_t type_id, Bytes info);
    std::uint8_t type_id;
    Bytes info;
};

// --- Nak reason codes -------------------------------------------------------
constexpr std::uint8_t NAK_UNKNOWN_CMD = 1;  // message type not understood
constexpr std::uint8_t NAK_BAD_ADDR = 2;     // address (or address+count) out of range
constexpr std::uint8_t NAK_BAD_LENGTH = 3;   // malformed request body
constexpr std::uint8_t NAK_READ_ONLY = 4;    // write to a read-only register

std::string nak_reason(std::uint8_t code);

// --- Messages ---------------------------------------------------------------

// Device -> host: one Euler attitude sample (streamed periodically).
// Fixed 20-byte body, big-endian.
struct AttitudeSample {
    static constexpr std::uint8_t TYPE_ID = 0x20;
    std::uint32_t seq = 0;   // increments per sample -> drop/gap detection
    std::uint32_t t_ms = 0;  // sensor timestamp, ms since start
    float roll = 0;          // degrees
    float pitch = 0;         // degrees
    float yaw = 0;           // degrees

    Bytes encode() const;
    static AttitudeSample unpack_body(const std::uint8_t* body, std::size_t n);
    bool operator==(const AttitudeSample& o) const {
        return seq == o.seq && t_ms == o.t_ms && roll == o.roll &&
               pitch == o.pitch && yaw == o.yaw;
    }
};

// Host -> device: read `count` registers starting at `addr`.
struct ReadRegister {
    static constexpr std::uint8_t TYPE_ID = 0x01;
    std::uint16_t addr = 0;
    std::uint8_t count = 0;

    Bytes encode() const;
    static ReadRegister unpack_body(const std::uint8_t* body, std::size_t n);
    bool operator==(const ReadRegister& o) const {
        return addr == o.addr && count == o.count;
    }
};

// Host -> device: write `value` to register `addr`.
struct WriteRegister {
    static constexpr std::uint8_t TYPE_ID = 0x02;
    std::uint16_t addr = 0;
    std::uint16_t value = 0;

    Bytes encode() const;
    static WriteRegister unpack_body(const std::uint8_t* body, std::size_t n);
    bool operator==(const WriteRegister& o) const {
        return addr == o.addr && value == o.value;
    }
};

// Device -> host: the register values for a preceding ReadRegister.
// Variable-length: `values` count is implied by the frame size.
struct ReadResponse {
    static constexpr std::uint8_t TYPE_ID = 0x81;
    std::uint16_t addr = 0;             // first register address (echo)
    std::vector<std::uint16_t> values;  // the register values

    Bytes encode() const;
    static ReadResponse unpack_body(const std::uint8_t* body, std::size_t n);
    bool operator==(const ReadResponse& o) const {
        return addr == o.addr && values == o.values;
    }
};

// Device -> host: confirms a WriteRegister; echoes the stored value.
struct WriteAck {
    static constexpr std::uint8_t TYPE_ID = 0x82;
    std::uint16_t addr = 0;
    std::uint16_t value = 0;
    std::uint8_t status = 0;  // 0 = OK

    Bytes encode() const;
    static WriteAck unpack_body(const std::uint8_t* body, std::size_t n);
    bool operator==(const WriteAck& o) const {
        return addr == o.addr && value == o.value && status == o.status;
    }
};

// Device -> host: a request was rejected; `code` says why.
struct Nak {
    static constexpr std::uint8_t TYPE_ID = 0xFF;
    std::uint8_t code = 0;    // one of NAK_*
    std::uint16_t addr = 0;   // offending address (0 if N/A)

    Bytes encode() const;
    static Nak unpack_body(const std::uint8_t* body, std::size_t n);
    std::string reason() const { return nak_reason(code); }
    bool operator==(const Nak& o) const {
        return code == o.code && addr == o.addr;
    }
};

}  // namespace serial_link::codec
