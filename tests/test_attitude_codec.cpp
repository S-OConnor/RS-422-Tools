// Mirrors tests/test_attitude_codec.py — size, round-trip, big-endian layout.
#include <gtest/gtest.h>

#include <cmath>

#include "serial_link/codec.hpp"

using namespace serial_link;
using namespace serial_link::codec;

TEST(AttitudeCodec, BodyIs20Bytes) {
    Bytes info = AttitudeSample{1, 2, 0, 0, 0}.encode();
    EXPECT_EQ(info.size(), 21u);  // 1 type-id + 20 body
    EXPECT_EQ(info[0], 0x20);
}

TEST(AttitudeCodec, RoundtripExactFloats) {
    AttitudeSample msg{1234, 56789, 12.5f, -3.25f, 180.0f};
    Message back = decode(msg.encode());
    ASSERT_TRUE(std::holds_alternative<AttitudeSample>(back));
    const auto& a = std::get<AttitudeSample>(back);
    EXPECT_EQ(a.seq, 1234u);
    EXPECT_EQ(a.t_ms, 56789u);
    EXPECT_EQ(a.roll, 12.5f);
    EXPECT_EQ(a.pitch, -3.25f);
    EXPECT_EQ(a.yaw, 180.0f);
}

TEST(AttitudeCodec, RoundtripInexactWithinF32) {
    AttitudeSample msg{0, 0, 15.0f, -7.123f, 359.9f};
    Message decoded = decode(msg.encode());
    const auto& a = std::get<AttitudeSample>(decoded);
    EXPECT_NEAR(a.pitch, -7.123f, 1e-3);
    EXPECT_NEAR(a.yaw, 359.9f, 1e-3);
}

TEST(AttitudeCodec, BigEndianWireLayout) {
    AttitudeSample msg{1, 2, 1.0f, 2.0f, 3.0f};
    // type_id + >IIfff : seq=1, t_ms=2, roll=1.0, pitch=2.0, yaw=3.0
    Bytes expected = {0x20,
                      0x00, 0x00, 0x00, 0x01,  // seq
                      0x00, 0x00, 0x00, 0x02,  // t_ms
                      0x3F, 0x80, 0x00, 0x00,  // 1.0f
                      0x40, 0x00, 0x00, 0x00,  // 2.0f
                      0x40, 0x40, 0x00, 0x00}; // 3.0f
    EXPECT_EQ(msg.encode(), expected);
}

TEST(AttitudeCodec, EqualityOperator) {
    EXPECT_EQ((AttitudeSample{1, 2, 3, 4, 5}), (AttitudeSample{1, 2, 3, 4, 5}));
    EXPECT_FALSE((AttitudeSample{1, 2, 3, 4, 5}) == (AttitudeSample{1, 2, 3, 4, 6}));
}
