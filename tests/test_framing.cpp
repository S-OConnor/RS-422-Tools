// Mirrors tests/test_framing.py — encode/decode round-trips, stuffing, split
// reads, leading garbage, shared flags, bad-FCS flagging, runt drop.
#include <gtest/gtest.h>

#include "serial_link/framing.hpp"

using namespace serial_link;
using namespace serial_link::framing;

namespace {
Frame roundtrip(const Bytes& info, std::uint8_t addr = DEFAULT_ADDRESS,
                std::uint8_t ctrl = DEFAULT_CONTROL) {
    FrameDecoder dec;
    auto frames = dec.feed(encode(info, addr, ctrl));
    EXPECT_EQ(frames.size(), 1u);
    return frames.at(0);
}
Bytes str(const char* s) { return Bytes(s, s + std::char_traits<char>::length(s)); }
}  // namespace

TEST(Framing, BasicRoundtrip) {
    Bytes info = {0x10, 'h', 'e', 'l', 'l', 'o'};
    Frame f = roundtrip(info);
    EXPECT_TRUE(f.fcs_ok);
    EXPECT_EQ(f.info, info);
    EXPECT_EQ(f.address, 0xFF);
    EXPECT_EQ(f.control, 0x03);
}

TEST(Framing, AddressAndControlPreserved) {
    Frame f = roundtrip({0x01, 0x02}, 0x42, 0x13);
    EXPECT_EQ(f.address, 0x42);
    EXPECT_EQ(f.control, 0x13);
    EXPECT_EQ(f.info, (Bytes{0x01, 0x02}));
}

TEST(Framing, StuffingOfSpecialOctets) {
    Bytes info = {FLAG, ESC, 0x00, 0x7E, 0x7D, 0x41};
    Bytes wire = encode(info);
    // No raw FLAG inside the frame body (between the delimiters).
    for (std::size_t i = 1; i + 1 < wire.size(); ++i) EXPECT_NE(wire[i], FLAG);
    EXPECT_EQ(roundtrip(info).info, info);
}

TEST(Framing, ControlCharsEscapedByDefault) {
    Bytes info;
    for (int i = 0; i < 0x20; ++i) info.push_back(static_cast<std::uint8_t>(i));
    info.push_back(0x10);
    Bytes wire = encode(info);
    for (std::size_t i = 1; i + 1 < wire.size(); ++i) EXPECT_NE(wire[i], FLAG);
    EXPECT_EQ(roundtrip(info).info, info);
}

TEST(Framing, DecoderHandlesSplitReads) {
    Bytes wire = encode(str("\x10" "abc"));
    FrameDecoder dec;
    std::vector<Frame> frames;
    for (std::uint8_t b : wire) {
        for (auto& f : dec.feed(Bytes{b})) frames.push_back(f);
    }
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].info, str("\x10" "abc"));
}

TEST(Framing, LeadingGarbageIgnored) {
    Bytes wire = str("\x00\x11garbage");
    Bytes framed = encode(str("\x10ok"));
    wire.insert(wire.end(), framed.begin(), framed.end());
    auto frames = FrameDecoder().feed(wire);
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].info, str("\x10ok"));
}

TEST(Framing, BackToBackAndSharedFlags) {
    Bytes a = encode(str("\x10one"));
    Bytes b = encode(str("\x10two"));
    Bytes stream(a.begin(), a.end() - 1);  // drop a's trailing flag (shared)
    stream.insert(stream.end(), b.begin(), b.end());
    auto frames = FrameDecoder().feed(stream);
    ASSERT_EQ(frames.size(), 2u);
    EXPECT_EQ(frames[0].info, str("\x10one"));
    EXPECT_EQ(frames[1].info, str("\x10two"));
}

TEST(Framing, BadFcsFlaggedNotDropped) {
    Bytes wire = encode(str("\x10payload"));
    wire[4] ^= 0x01;  // corrupt a body byte
    auto frames = FrameDecoder().feed(wire);
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_FALSE(frames[0].fcs_ok);
}

TEST(Framing, RuntFrameDropped) {
    auto frames = FrameDecoder().feed(Bytes{FLAG, 0x01, 0x02, FLAG});
    EXPECT_TRUE(frames.empty());
}
