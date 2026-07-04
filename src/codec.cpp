#include "serial_link/codec.hpp"

#include <cstdio>

namespace serial_link::codec {

Message decode(const Bytes& info) {
    if (info.empty()) throw CodecError("empty INFO field");
    std::uint8_t type_id = info[0];
    const std::uint8_t* body = info.data() + 1;
    std::size_t n = info.size() - 1;

    switch (type_id) {
        case AttitudeSample::TYPE_ID: return AttitudeSample::unpack_body(body, n);
        case ReadRegister::TYPE_ID: return ReadRegister::unpack_body(body, n);
        case WriteRegister::TYPE_ID: return WriteRegister::unpack_body(body, n);
        case ReadResponse::TYPE_ID: return ReadResponse::unpack_body(body, n);
        case WriteAck::TYPE_ID: return WriteAck::unpack_body(body, n);
        case Nak::TYPE_ID: return Nak::unpack_body(body, n);
        default: throw UnknownMessage(type_id, info);
    }
}

std::uint8_t type_id_of(const Message& msg) {
    return std::visit([](const auto& m) { return m.TYPE_ID; }, msg);
}

Bytes encode_message(const Message& msg) {
    return std::visit([](const auto& m) { return m.encode(); }, msg);
}

namespace {
std::string hex16(std::uint16_t v) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "0x%04X", v);
    return buf;
}
}  // namespace

std::string to_string(const Message& msg) {
    return std::visit(
        [](const auto& m) -> std::string {
            using T = std::decay_t<decltype(m)>;
            if constexpr (std::is_same_v<T, AttitudeSample>) {
                char buf[160];
                std::snprintf(buf, sizeof(buf),
                              "AttitudeSample(seq=%u, t_ms=%u, roll=%g, pitch=%g, yaw=%g)",
                              m.seq, m.t_ms, m.roll, m.pitch, m.yaw);
                return buf;
            } else if constexpr (std::is_same_v<T, ReadRegister>) {
                return "ReadRegister(addr=" + hex16(m.addr) +
                       ", count=" + std::to_string(m.count) + ")";
            } else if constexpr (std::is_same_v<T, WriteRegister>) {
                return "WriteRegister(addr=" + hex16(m.addr) +
                       ", value=" + hex16(m.value) + ")";
            } else if constexpr (std::is_same_v<T, ReadResponse>) {
                std::string s = "ReadResponse(addr=" + hex16(m.addr) + ", values=[";
                for (std::size_t i = 0; i < m.values.size(); ++i) {
                    if (i) s += ", ";
                    s += hex16(m.values[i]);
                }
                return s + "])";
            } else if constexpr (std::is_same_v<T, WriteAck>) {
                return "WriteAck(addr=" + hex16(m.addr) + ", value=" + hex16(m.value) +
                       ", status=" + std::to_string(m.status) + ")";
            } else {  // Nak
                return "Nak(code=" + std::to_string(m.code) + ", addr=" + hex16(m.addr) +
                       ")";
            }
        },
        msg);
}

}  // namespace serial_link::codec
