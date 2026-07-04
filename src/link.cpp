#include "serial_link/link.hpp"

#include <chrono>

namespace serial_link {

FramedLink::FramedLink(std::unique_ptr<Transport> transport, std::size_t read_size)
    : transport_(std::move(transport)), read_size_(read_size) {}

void FramedLink::send_info(const Bytes& info, std::uint8_t address, std::uint8_t control) {
    transport_->write(framing::encode(info, address, control));
}

ReceivedFrame FramedLink::wrap(framing::Frame frame) {
    ReceivedFrame rf;
    if (!frame.fcs_ok) {
        rf.error = "bad FCS";
        rf.frame = std::move(frame);
        return rf;
    }
    try {
        rf.message = codec::decode(frame.info);
    } catch (const codec::CodecError& exc) {
        rf.error = exc.what();
    }
    rf.frame = std::move(frame);
    return rf;
}

std::optional<ReceivedFrame> FramedLink::next(std::optional<double> timeout) {
    if (!inbox_.empty()) {
        ReceivedFrame rf = std::move(inbox_.front());
        inbox_.pop_front();
        return rf;
    }

    using clock = std::chrono::steady_clock;
    std::optional<clock::time_point> deadline;
    if (timeout) {
        deadline = clock::now() +
                   std::chrono::duration_cast<clock::duration>(
                       std::chrono::duration<double>(*timeout));
    }

    while (true) {
        Bytes data = transport_->read(read_size_);
        if (!data.empty()) {
            for (auto& frame : decoder_.feed(data)) inbox_.push_back(wrap(std::move(frame)));
            if (!inbox_.empty()) {
                ReceivedFrame rf = std::move(inbox_.front());
                inbox_.pop_front();
                return rf;
            }
        } else if (transport_->eof) {
            return std::nullopt;
        }
        if (deadline && clock::now() >= *deadline) return std::nullopt;
    }
}

void FramedLink::close() {
    if (transport_) transport_->close();
}

}  // namespace serial_link
