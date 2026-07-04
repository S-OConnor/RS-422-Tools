#include "serial_link/messages.hpp"

#include "serial_link/common.hpp"

namespace serial_link::codec {

using detail::get_f32be;
using detail::get_u16be;
using detail::get_u32be;
using detail::put_f32be;
using detail::put_u16be;
using detail::put_u32be;
using detail::put_u8;

UnknownMessage::UnknownMessage(std::uint8_t tid, Bytes inf)
    : CodecError("unknown message type id 0x" +
                 std::string{"0123456789ABCDEF"[(tid >> 4) & 0xF],
                             "0123456789ABCDEF"[tid & 0xF]}),
      type_id(tid),
      info(std::move(inf)) {}

std::string nak_reason(std::uint8_t code) {
    switch (code) {
        case NAK_UNKNOWN_CMD: return "unknown command";
        case NAK_BAD_ADDR: return "bad address";
        case NAK_BAD_LENGTH: return "bad length";
        case NAK_READ_ONLY: return "read-only";
        default: return "code " + std::to_string(code);
    }
}

namespace {
// Fixed-size messages check their body length exactly, matching the Python
// codec's CodecError on a size mismatch.
void require_size(const char* name, std::size_t got, std::size_t want) {
    if (got != want) {
        throw CodecError(std::string(name) + ": body is " + std::to_string(got) +
                         " bytes, expected " + std::to_string(want));
    }
}
}  // namespace

// --- AttitudeSample (type 0x20, 20-byte body) -------------------------------
Bytes AttitudeSample::encode() const {
    Bytes b;
    b.reserve(21);
    put_u8(b, TYPE_ID);
    put_u32be(b, seq);
    put_u32be(b, t_ms);
    put_f32be(b, roll);
    put_f32be(b, pitch);
    put_f32be(b, yaw);
    return b;
}

AttitudeSample AttitudeSample::unpack_body(const std::uint8_t* p, std::size_t n) {
    require_size("AttitudeSample", n, 20);
    AttitudeSample m;
    m.seq = get_u32be(p);
    m.t_ms = get_u32be(p + 4);
    m.roll = get_f32be(p + 8);
    m.pitch = get_f32be(p + 12);
    m.yaw = get_f32be(p + 16);
    return m;
}

// --- ReadRegister (type 0x01, 3-byte body) ----------------------------------
Bytes ReadRegister::encode() const {
    Bytes b;
    b.reserve(4);
    put_u8(b, TYPE_ID);
    put_u16be(b, addr);
    put_u8(b, count);
    return b;
}

ReadRegister ReadRegister::unpack_body(const std::uint8_t* p, std::size_t n) {
    require_size("ReadRegister", n, 3);
    ReadRegister m;
    m.addr = get_u16be(p);
    m.count = p[2];
    return m;
}

// --- WriteRegister (type 0x02, 4-byte body) ---------------------------------
Bytes WriteRegister::encode() const {
    Bytes b;
    b.reserve(5);
    put_u8(b, TYPE_ID);
    put_u16be(b, addr);
    put_u16be(b, value);
    return b;
}

WriteRegister WriteRegister::unpack_body(const std::uint8_t* p, std::size_t n) {
    require_size("WriteRegister", n, 4);
    WriteRegister m;
    m.addr = get_u16be(p);
    m.value = get_u16be(p + 2);
    return m;
}

// --- ReadResponse (type 0x81, variable-length body) -------------------------
Bytes ReadResponse::encode() const {
    Bytes b;
    b.reserve(3 + values.size() * 2);
    put_u8(b, TYPE_ID);
    put_u16be(b, addr);
    for (std::uint16_t v : values) put_u16be(b, v);
    return b;
}

ReadResponse ReadResponse::unpack_body(const std::uint8_t* p, std::size_t n) {
    if (n < 2) {
        throw CodecError("ReadResponse: body is " + std::to_string(n) +
                         " bytes, expected at least 2");
    }
    std::size_t rest = n - 2;
    if (rest % 2 != 0) {
        throw CodecError("ReadResponse: trailing " + std::to_string(rest) +
                         " bytes not a multiple of element size 2");
    }
    ReadResponse m;
    m.addr = get_u16be(p);
    m.values.reserve(rest / 2);
    for (std::size_t i = 2; i < n; i += 2) m.values.push_back(get_u16be(p + i));
    return m;
}

// --- WriteAck (type 0x82, 5-byte body) --------------------------------------
Bytes WriteAck::encode() const {
    Bytes b;
    b.reserve(6);
    put_u8(b, TYPE_ID);
    put_u16be(b, addr);
    put_u16be(b, value);
    put_u8(b, status);
    return b;
}

WriteAck WriteAck::unpack_body(const std::uint8_t* p, std::size_t n) {
    require_size("WriteAck", n, 5);
    WriteAck m;
    m.addr = get_u16be(p);
    m.value = get_u16be(p + 2);
    m.status = p[4];
    return m;
}

// --- Nak (type 0xFF, 3-byte body) -------------------------------------------
Bytes Nak::encode() const {
    Bytes b;
    b.reserve(4);
    put_u8(b, TYPE_ID);
    put_u8(b, code);
    put_u16be(b, addr);
    return b;
}

Nak Nak::unpack_body(const std::uint8_t* p, std::size_t n) {
    require_size("Nak", n, 3);
    Nak m;
    m.code = p[0];
    m.addr = get_u16be(p + 1);
    return m;
}

}  // namespace serial_link::codec
