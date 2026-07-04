// RFC 1662 FCS-16 (HDLC/PPP frame check sequence).
//
// The 16-bit FCS uses the CRC-CCITT polynomial x^16 + x^12 + x^5 + 1 in its
// reflected form (0x8408), exactly as specified in RFC 1662 Appendix C. The
// 256-entry lookup table is generated at runtime (not transcribed) to remove
// any chance of a copy error — same approach as the Python original.
//
// Wire conventions (RFC 1662):
//   * the running FCS is seeded with 0xFFFF;
//   * the value transmitted is the ones-complement of the running FCS, sent
//     low byte first (see frame_fcs);
//   * a receiver that runs fcs16 over `info + fcs` gets the constant "good FCS"
//     residual 0xF0B8 when the frame is intact (see check_fcs).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "serial_link/common.hpp"

namespace serial_link::fcs {

constexpr std::uint16_t INIT_FCS16 = 0xFFFF;
constexpr std::uint16_t GOOD_FCS16 = 0xF0B8;

// The generated 256-entry FCS table (built once on first use).
const std::array<std::uint16_t, 256>& table();

// Running FCS-16 over `data` (not complemented). `fcs` may be passed to
// continue a computation across chunks.
std::uint16_t fcs16(const std::uint8_t* data, std::size_t n, std::uint16_t fcs = INIT_FCS16);
std::uint16_t fcs16(const Bytes& data, std::uint16_t fcs = INIT_FCS16);

// The 2 FCS bytes to append after `data`, low byte first.
Bytes frame_fcs(const Bytes& data);

// True if `data_with_fcs` (payload followed by its 2 FCS bytes) is intact.
bool check_fcs(const Bytes& data_with_fcs);

}  // namespace serial_link::fcs
