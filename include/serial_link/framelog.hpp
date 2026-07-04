// Shared logging helpers: render frames/bytes for humans.
#pragma once

#include <string>

#include "serial_link/common.hpp"
#include "serial_link/link.hpp"

namespace serial_link::framelog {

// Space-separated hex, e.g. "7E FF 03 10 ...".
std::string hexdump(const Bytes& data);

// One-line summary of a ReceivedFrame.
std::string format_received(const ReceivedFrame& received);

}  // namespace serial_link::framelog
