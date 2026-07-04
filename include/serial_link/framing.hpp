// L1 — RFC 1662 HDLC-like framing.
//
// A frame on the wire looks like:
//     FLAG | address | control | INFO | FCS-16 | FLAG
//
// FLAG (0x7E) delimits frames. Within a frame, octet transparency is applied:
// any FLAG, ESC (0x7D), or control octet (< 0x20) is sent as ESC, octet ^ 0x20.
// The FCS-16 covers address + control + INFO.
//
// This layer is transport-agnostic: encode() turns an INFO field into wire
// bytes, and FrameDecoder turns an arbitrary byte stream back into frames.
#pragma once

#include <cstdint>
#include <vector>

#include "serial_link/common.hpp"

namespace serial_link::framing {

constexpr std::uint8_t FLAG = 0x7E;
constexpr std::uint8_t ESC = 0x7D;
constexpr std::uint8_t XOR = 0x20;

// PPP defaults: All-Stations address + Unnumbered Information control.
constexpr std::uint8_t DEFAULT_ADDRESS = 0xFF;
constexpr std::uint8_t DEFAULT_CONTROL = 0x03;

// A decoded frame. `fcs_ok` lets callers see (and log) corrupt frames rather
// than silently dropping them.
struct Frame {
    std::uint8_t address = 0;
    std::uint8_t control = 0;
    Bytes info;
    bool fcs_ok = false;
};

// Frame an INFO field into wire bytes (with delimiting flags and FCS). Control
// octets (< 0x20) are escaped, matching the Python encoder's default ACCM.
Bytes encode(const Bytes& info,
             std::uint8_t address = DEFAULT_ADDRESS,
             std::uint8_t control = DEFAULT_CONTROL);

// Streaming de-framer. Feed it arbitrary byte chunks; get back frames.
//
// Tolerates split reads, leading garbage before the first flag, and shared or
// back-to-back flags between frames. Frames shorter than the minimum
// (address + control + 2 FCS bytes) are treated as runts/idle and dropped.
class FrameDecoder {
public:
    std::vector<Frame> feed(const Bytes& data);
    std::vector<Frame> feed(const std::uint8_t* data, std::size_t n);

private:
    static constexpr std::size_t MIN_LEN = 4;  // address + control + 2 FCS bytes

    bool finish(Frame& out);  // returns false for runts

    Bytes buf_;
    bool in_frame_ = false;
    bool esc_ = false;
};

}  // namespace serial_link::framing
