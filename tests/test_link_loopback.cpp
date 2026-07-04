// Mirrors tests/test_request_response.py — a c2-style host issues requests, a
// minimal in-thread device answers, over a real socket pair. Also covers the
// request-timeout path. Exercises FramedLink send/receive/request end to end.
#include <gtest/gtest.h>
#include <sys/socket.h>

#include <map>
#include <thread>

#include "serial_link/link.hpp"
#include "serial_link/transport.hpp"

using namespace serial_link;
using namespace serial_link::codec;

namespace {

// A tiny stand-in device: a flat register bank + the request->reply mapping.
// (The full device_sim stays in Python; this is just enough to drive the link.)
struct Device {
    std::map<std::uint16_t, std::uint16_t> regs;
    std::uint16_t size = 64;

    std::optional<Message> respond(const Message& msg) {
        if (const auto* r = std::get_if<ReadRegister>(&msg)) {
            if (r->addr + r->count > size) return Nak{NAK_BAD_ADDR, r->addr};
            ReadResponse rr;
            rr.addr = r->addr;
            for (int i = 0; i < r->count; ++i) rr.values.push_back(regs[r->addr + i]);
            return rr;
        }
        if (const auto* w = std::get_if<WriteRegister>(&msg)) {
            if (w->addr >= size) return Nak{NAK_BAD_ADDR, w->addr};
            regs[w->addr] = w->value;
            return WriteAck{w->addr, w->value, 0};
        }
        return std::nullopt;
    }
};

std::pair<int, int> make_socketpair() {
    int fds[2];
    EXPECT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
    return {fds[0], fds[1]};
}

}  // namespace

TEST(LinkLoopback, C2ReadWriteCycle) {
    auto [a, b] = make_socketpair();
    FramedLink host(std::make_unique<TcpTransport>(a));
    FramedLink dev(std::make_unique<TcpTransport>(b));

    Device device;
    device.regs[0x10] = 0x1111;

    std::thread t([&dev, &device] {
        while (auto rf = dev.next()) {
            if (!rf->ok()) continue;
            if (auto reply = device.respond(*rf->message)) {
                std::visit([&dev](const auto& m) { dev.send(m); }, *reply);
            }
        }
    });

    // read seeded register
    auto r = host.request(ReadRegister{0x10, 1}, 2.0);
    ASSERT_TRUE(r && r->ok());
    ASSERT_TRUE(std::holds_alternative<ReadResponse>(*r->message));
    EXPECT_EQ(std::get<ReadResponse>(*r->message).values, (std::vector<std::uint16_t>{0x1111}));

    // write then read back
    auto w = host.request(WriteRegister{0x20, 0xBEEF}, 2.0);
    ASSERT_TRUE(w && w->ok());
    ASSERT_TRUE(std::holds_alternative<WriteAck>(*w->message));
    EXPECT_EQ(std::get<WriteAck>(*w->message).value, 0xBEEF);

    auto r2 = host.request(ReadRegister{0x20, 1}, 2.0);
    EXPECT_EQ(std::get<ReadResponse>(*r2->message).values, (std::vector<std::uint16_t>{0xBEEF}));

    // multi-register read
    auto r3 = host.request(ReadRegister{0x10, 3}, 2.0);
    EXPECT_EQ(std::get<ReadResponse>(*r3->message).values.size(), 3u);

    // bad address -> Nak
    auto bad = host.request(ReadRegister{100, 10}, 2.0);
    ASSERT_TRUE(bad && bad->ok());
    EXPECT_TRUE(std::holds_alternative<Nak>(*bad->message));

    host.close();  // device's next() hits EOF -> thread exits
    t.join();
    dev.close();
}

TEST(LinkLoopback, RequestTimeoutReturnsNullopt) {
    auto [a, b] = make_socketpair();
    FramedLink host(std::make_unique<TcpTransport>(a));
    auto reply = host.request(ReadRegister{0, 1}, 0.2);
    EXPECT_FALSE(reply.has_value());
    host.close();
    ::close(b);
}
