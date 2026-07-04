// Common types + big-endian (network order) byte helpers shared by the codec.
//
// The wire format is defined once in the Python `serial_link` package; this C++
// port must stay byte-for-byte identical to it so the two interoperate (the
// sensor_sim / device_sim simulators stay in Python). Everything on the wire is
// big-endian, matching Python's `struct` ">" format.
#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

namespace serial_link {

// A raw byte buffer — the currency between transport, framing, and codec.
using Bytes = std::vector<std::uint8_t>;

namespace detail {

static_assert(sizeof(float) == 4, "AttitudeSample assumes IEEE-754 32-bit float");

inline void put_u8(Bytes& b, std::uint8_t v) { b.push_back(v); }

inline void put_u16be(Bytes& b, std::uint16_t v) {
    b.push_back(static_cast<std::uint8_t>(v >> 8));
    b.push_back(static_cast<std::uint8_t>(v & 0xFF));
}

inline void put_u32be(Bytes& b, std::uint32_t v) {
    b.push_back(static_cast<std::uint8_t>(v >> 24));
    b.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    b.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    b.push_back(static_cast<std::uint8_t>(v & 0xFF));
}

inline void put_f32be(Bytes& b, float f) {
    std::uint32_t u;
    std::memcpy(&u, &f, sizeof(u));
    put_u32be(b, u);
}

inline std::uint16_t get_u16be(const std::uint8_t* p) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(p[0]) << 8) | p[1]);
}

inline std::uint32_t get_u32be(const std::uint8_t* p) {
    return (static_cast<std::uint32_t>(p[0]) << 24) |
           (static_cast<std::uint32_t>(p[1]) << 16) |
           (static_cast<std::uint32_t>(p[2]) << 8) |
           static_cast<std::uint32_t>(p[3]);
}

inline float get_f32be(const std::uint8_t* p) {
    std::uint32_t u = get_u32be(p);
    float f;
    std::memcpy(&f, &u, sizeof(f));
    return f;
}

}  // namespace detail
}  // namespace serial_link
