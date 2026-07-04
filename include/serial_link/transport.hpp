// L0 — the Transport seam.
//
// Transport is the one interface everything above it (framing, codec, link,
// apps) is written against. Nothing above this layer knows or cares whether the
// bytes come from the real RS-422 cable or a hardware-free stand-in.
//
// read() returns whatever bytes are available (possibly empty on timeout) and
// never blocks forever. `eof` reports that the far end has closed, so a receive
// loop can terminate instead of spinning.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "serial_link/common.hpp"

namespace serial_link {

// Bidirectional byte pipe.
class Transport {
public:
    virtual ~Transport() = default;

    // Return up to `n` bytes; empty if none are available right now.
    virtual Bytes read(std::size_t n = 4096) = 0;

    // Write all of `data`; return the number of bytes written.
    virtual std::size_t write(const std::uint8_t* data, std::size_t n) = 0;
    std::size_t write(const Bytes& data) { return write(data.data(), data.size()); }

    virtual void close() {}

    // True once the peer has closed / the device is gone.
    bool eof = false;
};

// The real RS-422/485 cable — the only transport that touches hardware.
// The Adafruit FT231X cable, 8N1, opened raw via termios.
class SerialTransport : public Transport {
public:
    explicit SerialTransport(const std::string& port, int baud = 115200,
                             double timeout = 0.1);
    ~SerialTransport() override;

    Bytes read(std::size_t n = 4096) override;
    std::size_t write(const std::uint8_t* data, std::size_t n) override;
    void close() override;

private:
    int fd_ = -1;
};

// A local byte pipe over a (connected) TCP socket — a stand-in for the wire.
// The TCP here is just a convenient on-host pipe, not networking.
class TcpTransport : public Transport {
public:
    // Take ownership of an already-connected socket fd (e.g. socketpair()).
    explicit TcpTransport(int fd, double timeout = 0.1);
    ~TcpTransport() override;

    static std::unique_ptr<TcpTransport> connect(const std::string& host, int port);
    // Block until one peer connects, then return a transport for it.
    static std::unique_ptr<TcpTransport> listen_one(const std::string& host, int port);

    Bytes read(std::size_t n = 4096) override;
    std::size_t write(const std::uint8_t* data, std::size_t n) override;
    void close() override;

private:
    int fd_ = -1;
};

// A pseudo-terminal standing in for a serial port. Owns the master side;
// `slave_name` is a /dev/pts path another process (or pyserial) can open
// exactly as if it were a serial port.
class PtyTransport : public Transport {
public:
    PtyTransport();
    ~PtyTransport() override;

    Bytes read(std::size_t n = 4096) override;
    std::size_t write(const std::uint8_t* data, std::size_t n) override;
    void close() override;

    std::string slave_name;

private:
    int master_ = -1;
};

}  // namespace serial_link
