#include "serial_link/transport.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pty.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

namespace serial_link {

namespace {

// Map an integer baud to the matching termios speed constant. Falls back to
// B115200 (the cable's default) for an unrecognised value.
speed_t baud_constant(int baud) {
    switch (baud) {
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
        case 460800: return B460800;
        case 921600: return B921600;
        default: return B115200;
    }
}

// Put a tty fd into raw 8N1 with a read timeout of `timeout` seconds.
void configure_raw_tty(int fd, speed_t speed, double timeout) {
    struct termios tio {};
    if (tcgetattr(fd, &tio) != 0) {
        throw std::runtime_error(std::string("tcgetattr: ") + std::strerror(errno));
    }
    cfmakeraw(&tio);
    tio.c_cflag |= (CLOCAL | CREAD);
    tio.c_cflag &= ~static_cast<tcflag_t>(CSTOPB);   // 1 stop bit
    tio.c_cflag &= ~static_cast<tcflag_t>(PARENB);   // no parity
    tio.c_cflag &= ~static_cast<tcflag_t>(CRTSCTS);  // no flow control
    if (speed != 0) {
        cfsetispeed(&tio, speed);
        cfsetospeed(&tio, speed);
    }
    // Non-canonical: return available bytes; VTIME bounds the wait (tenths of s).
    tio.c_cc[VMIN] = 0;
    int deciseconds = static_cast<int>(timeout * 10.0);
    tio.c_cc[VTIME] = static_cast<cc_t>(deciseconds < 1 ? 1 : deciseconds);
    if (tcsetattr(fd, TCSANOW, &tio) != 0) {
        throw std::runtime_error(std::string("tcsetattr: ") + std::strerror(errno));
    }
}

std::size_t write_all(int fd, const std::uint8_t* data, std::size_t n) {
    std::size_t off = 0;
    while (off < n) {
        ssize_t w = ::write(fd, data + off, n - off);
        if (w < 0) {
            if (errno == EINTR) continue;
            throw std::runtime_error(std::string("write: ") + std::strerror(errno));
        }
        off += static_cast<std::size_t>(w);
    }
    return off;
}

}  // namespace

// --- SerialTransport --------------------------------------------------------
SerialTransport::SerialTransport(const std::string& port, int baud, double timeout) {
    fd_ = ::open(port.c_str(), O_RDWR | O_NOCTTY);
    if (fd_ < 0) {
        throw std::runtime_error("open " + port + ": " + std::strerror(errno));
    }
    configure_raw_tty(fd_, baud_constant(baud), timeout);
}

SerialTransport::~SerialTransport() { close(); }

Bytes SerialTransport::read(std::size_t n) {
    Bytes buf(n);
    ssize_t r = ::read(fd_, buf.data(), n);
    if (r < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return {};
        eof = true;
        return {};
    }
    buf.resize(static_cast<std::size_t>(r));
    return buf;
}

std::size_t SerialTransport::write(const std::uint8_t* data, std::size_t n) {
    return write_all(fd_, data, n);
}

void SerialTransport::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

// --- TcpTransport -----------------------------------------------------------
namespace {
void set_recv_timeout(int fd, double timeout) {
    struct timeval tv {};
    tv.tv_sec = static_cast<time_t>(timeout);
    tv.tv_usec = static_cast<suseconds_t>((timeout - static_cast<double>(tv.tv_sec)) * 1e6);
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}
}  // namespace

TcpTransport::TcpTransport(int fd, double timeout) : fd_(fd) {
    set_recv_timeout(fd_, timeout);
}

TcpTransport::~TcpTransport() { close(); }

std::unique_ptr<TcpTransport> TcpTransport::connect(const std::string& host, int port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) throw std::runtime_error(std::string("socket: ") + std::strerror(errno));

    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<std::uint16_t>(port));
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        // Fall back to a name lookup for non-dotted hosts (e.g. "localhost").
        struct addrinfo hints {};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        struct addrinfo* res = nullptr;
        if (::getaddrinfo(host.c_str(), nullptr, &hints, &res) != 0 || !res) {
            ::close(fd);
            throw std::runtime_error("cannot resolve host: " + host);
        }
        addr.sin_addr = reinterpret_cast<struct sockaddr_in*>(res->ai_addr)->sin_addr;
        ::freeaddrinfo(res);
    }
    if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        ::close(fd);
        throw std::runtime_error("connect " + host + ":" + std::to_string(port) + ": " +
                                 std::strerror(errno));
    }
    return std::make_unique<TcpTransport>(fd);
}

std::unique_ptr<TcpTransport> TcpTransport::listen_one(const std::string& host, int port) {
    int srv = ::socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) throw std::runtime_error(std::string("socket: ") + std::strerror(errno));
    int one = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<std::uint16_t>(port));
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    }
    if (::bind(srv, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        ::close(srv);
        throw std::runtime_error("bind " + host + ":" + std::to_string(port) + ": " +
                                 std::strerror(errno));
    }
    if (::listen(srv, 1) != 0) {
        ::close(srv);
        throw std::runtime_error(std::string("listen: ") + std::strerror(errno));
    }
    int conn = ::accept(srv, nullptr, nullptr);
    ::close(srv);
    if (conn < 0) throw std::runtime_error(std::string("accept: ") + std::strerror(errno));
    return std::make_unique<TcpTransport>(conn);
}

Bytes TcpTransport::read(std::size_t n) {
    Bytes buf(n);
    ssize_t r = ::recv(fd_, buf.data(), n, 0);
    if (r < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return {};
        eof = true;
        return {};
    }
    if (r == 0) {
        eof = true;
        return {};
    }
    buf.resize(static_cast<std::size_t>(r));
    return buf;
}

std::size_t TcpTransport::write(const std::uint8_t* data, std::size_t n) {
    return write_all(fd_, data, n);
}

void TcpTransport::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

// --- PtyTransport -----------------------------------------------------------
PtyTransport::PtyTransport() {
    int slave = -1;
    if (::openpty(&master_, &slave, nullptr, nullptr, nullptr) != 0) {
        throw std::runtime_error(std::string("openpty: ") + std::strerror(errno));
    }
    char name[256];
    if (::ptsname_r(master_, name, sizeof(name)) != 0) {
        ::close(master_);
        ::close(slave);
        throw std::runtime_error(std::string("ptsname_r: ") + std::strerror(errno));
    }
    slave_name = name;

    // Raw mode on both ends: no echo / line discipline mangling binary bytes.
    struct termios tio {};
    if (tcgetattr(master_, &tio) == 0) {
        cfmakeraw(&tio);
        tcsetattr(master_, TCSANOW, &tio);
    }
    if (tcgetattr(slave, &tio) == 0) {
        cfmakeraw(&tio);
        tcsetattr(slave, TCSANOW, &tio);
    }
    ::close(slave);  // the pts path stays openable by the peer
    int flags = fcntl(master_, F_GETFL, 0);
    fcntl(master_, F_SETFL, flags | O_NONBLOCK);
}

PtyTransport::~PtyTransport() { close(); }

Bytes PtyTransport::read(std::size_t n) {
    Bytes buf(n);
    ssize_t r = ::read(master_, buf.data(), n);
    if (r < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return {};
        eof = true;
        return {};
    }
    buf.resize(static_cast<std::size_t>(r));
    return buf;
}

std::size_t PtyTransport::write(const std::uint8_t* data, std::size_t n) {
    return write_all(master_, data, n);
}

void PtyTransport::close() {
    if (master_ >= 0) {
        ::close(master_);
        master_ = -1;
    }
}

}  // namespace serial_link
