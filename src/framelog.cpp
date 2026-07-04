#include "serial_link/framelog.hpp"

#include <cstdio>

#include "serial_link/codec.hpp"

namespace serial_link::framelog {

std::string hexdump(const Bytes& data) {
    std::string out;
    out.reserve(data.size() * 3);
    char buf[4];
    for (std::size_t i = 0; i < data.size(); ++i) {
        if (i) out += ' ';
        std::snprintf(buf, sizeof(buf), "%02X", data[i]);
        out += buf;
    }
    return out;
}

std::string format_received(const ReceivedFrame& received) {
    char addr[8];
    std::snprintf(addr, sizeof(addr), "0x%02X", received.frame.address);
    std::string info_hex = hexdump(received.frame.info);
    if (received.ok()) {
        return "addr=" + std::string(addr) + "  " + codec::to_string(*received.message) +
               "  [" + info_hex + "]";
    }
    return "addr=" + std::string(addr) + "  !! " + received.error + "  [" + info_hex + "]";
}

}  // namespace serial_link::framelog
