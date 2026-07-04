#include "serial_link/fcs.hpp"

namespace serial_link::fcs {

const std::array<std::uint16_t, 256>& table() {
    // Generated once from the reflected polynomial 0x8408 (RFC 1662 App. C).
    static const std::array<std::uint16_t, 256> tab = [] {
        std::array<std::uint16_t, 256> t{};
        for (int byte = 0; byte < 256; ++byte) {
            std::uint16_t crc = static_cast<std::uint16_t>(byte);
            for (int i = 0; i < 8; ++i) {
                crc = (crc & 1) ? static_cast<std::uint16_t>((crc >> 1) ^ 0x8408)
                                : static_cast<std::uint16_t>(crc >> 1);
            }
            t[static_cast<std::size_t>(byte)] = crc;
        }
        return t;
    }();
    return tab;
}

std::uint16_t fcs16(const std::uint8_t* data, std::size_t n, std::uint16_t fcs) {
    const auto& tab = table();
    for (std::size_t i = 0; i < n; ++i) {
        fcs = static_cast<std::uint16_t>((fcs >> 8) ^ tab[(fcs ^ data[i]) & 0xFF]);
    }
    return fcs;
}

std::uint16_t fcs16(const Bytes& data, std::uint16_t fcs) {
    return fcs16(data.data(), data.size(), fcs);
}

Bytes frame_fcs(const Bytes& data) {
    std::uint16_t fcs = static_cast<std::uint16_t>(fcs16(data) ^ 0xFFFF);
    return Bytes{static_cast<std::uint8_t>(fcs & 0xFF),
                 static_cast<std::uint8_t>((fcs >> 8) & 0xFF)};
}

bool check_fcs(const Bytes& data_with_fcs) {
    return fcs16(data_with_fcs) == GOOD_FCS16;
}

}  // namespace serial_link::fcs
