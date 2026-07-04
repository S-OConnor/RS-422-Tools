#include "serial_link/framing.hpp"

#include "serial_link/fcs.hpp"

namespace serial_link::framing {

namespace {
// A byte must be escaped if it is a FLAG/ESC or a control octet (< 0x20). The
// Python encoder's default ACCM (accm=None) escapes all control chars — the
// conservative choice — and we mirror it exactly.
bool must_escape(std::uint8_t b) {
    return b == FLAG || b == ESC || b < 0x20;
}
}  // namespace

Bytes encode(const Bytes& info, std::uint8_t address, std::uint8_t control) {
    Bytes payload;
    payload.reserve(info.size() + 4);
    payload.push_back(address);
    payload.push_back(control);
    payload.insert(payload.end(), info.begin(), info.end());
    Bytes fcs = fcs::frame_fcs(payload);
    payload.insert(payload.end(), fcs.begin(), fcs.end());

    Bytes out;
    out.reserve(payload.size() + 2);
    out.push_back(FLAG);
    for (std::uint8_t b : payload) {
        if (must_escape(b)) {
            out.push_back(ESC);
            out.push_back(static_cast<std::uint8_t>(b ^ XOR));
        } else {
            out.push_back(b);
        }
    }
    out.push_back(FLAG);
    return out;
}

std::vector<Frame> FrameDecoder::feed(const Bytes& data) {
    return feed(data.data(), data.size());
}

std::vector<Frame> FrameDecoder::feed(const std::uint8_t* data, std::size_t n) {
    std::vector<Frame> frames;
    for (std::size_t i = 0; i < n; ++i) {
        std::uint8_t byte = data[i];
        if (byte == FLAG) {
            if (in_frame_ && !buf_.empty()) {
                Frame frame;
                if (finish(frame)) frames.push_back(std::move(frame));
            }
            buf_.clear();
            in_frame_ = true;
            esc_ = false;
        } else if (!in_frame_) {
            continue;  // garbage before the first flag
        } else if (byte == ESC) {
            esc_ = true;
        } else if (esc_) {
            buf_.push_back(static_cast<std::uint8_t>(byte ^ XOR));
            esc_ = false;
        } else {
            buf_.push_back(byte);
        }
    }
    return frames;
}

bool FrameDecoder::finish(Frame& out) {
    if (buf_.size() < MIN_LEN) return false;  // runt / idle
    out.address = buf_[0];
    out.control = buf_[1];
    out.info.assign(buf_.begin() + 2, buf_.end() - 2);
    out.fcs_ok = fcs::check_fcs(buf_);
    return true;
}

}  // namespace serial_link::framing
