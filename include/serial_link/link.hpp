// L3 — FramedLink: ties a Transport + framing + codec together.
//
// This is the opt-in convenience layer. It forces no concurrency model: next()
// is a plain blocking read and request() is a blocking send-then-await, so an
// app can drive either from the main thread, a worker thread, or wrap it however
// it likes.
#pragma once

#include <cstddef>
#include <deque>
#include <memory>
#include <optional>
#include <string>

#include "serial_link/codec.hpp"
#include "serial_link/framing.hpp"
#include "serial_link/transport.hpp"

namespace serial_link {

// A frame as it came off the link, with decode lazily attempted.
//
// `message` holds the decoded codec::Message, or is empty if the FCS failed or
// the type id was unknown — `error` then says why. Either way the raw `frame`
// is available for logging.
struct ReceivedFrame {
    framing::Frame frame;
    std::optional<codec::Message> message;
    std::string error;

    bool ok() const { return message.has_value(); }
};

class FramedLink {
public:
    explicit FramedLink(std::unique_ptr<Transport> transport, std::size_t read_size = 4096);

    // Frame and transmit one message (any catalog struct with encode()).
    template <class M>
    void send(const M& message,
              std::uint8_t address = framing::DEFAULT_ADDRESS,
              std::uint8_t control = framing::DEFAULT_CONTROL) {
        send_info(message.encode(), address, control);
    }

    // Return one ReceivedFrame, or std::nullopt on EOF/timeout.
    //
    // No timeout (the default) blocks until a frame arrives or the transport
    // reaches EOF — this is the receive loop: `while (auto rf = link.next())`.
    // A timeout bounds the wait (used by request()).
    std::optional<ReceivedFrame> next(std::optional<double> timeout = std::nullopt);

    // Send `message` and return the next received frame (the reply), or
    // std::nullopt if none arrived within `timeout` seconds. Correlation is
    // implicit (the next frame is the reply) — valid on a point-to-point bus.
    template <class M>
    std::optional<ReceivedFrame> request(const M& message, double timeout = 1.0,
                                         std::uint8_t address = framing::DEFAULT_ADDRESS,
                                         std::uint8_t control = framing::DEFAULT_CONTROL) {
        send(message, address, control);
        return next(timeout);
    }

    // True once the underlying transport has reached EOF. Lets a bounded-timeout
    // receive loop tell "no data yet" apart from "peer gone".
    bool at_eof() const { return transport_->eof; }

    void close();

private:
    void send_info(const Bytes& info, std::uint8_t address, std::uint8_t control);
    static ReceivedFrame wrap(framing::Frame frame);

    std::unique_ptr<Transport> transport_;
    framing::FrameDecoder decoder_;
    std::size_t read_size_;
    std::deque<ReceivedFrame> inbox_;
};

}  // namespace serial_link
