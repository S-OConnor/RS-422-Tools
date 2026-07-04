// Mirrors tests/test_fcs.py — FCS-16 table, good residual, corruption rejection.
#include <gtest/gtest.h>

#include "serial_link/fcs.hpp"

using namespace serial_link;
using serial_link::fcs::check_fcs;
using serial_link::fcs::fcs16;
using serial_link::fcs::frame_fcs;

TEST(Fcs, TableHas256EntriesInRange) {
    const auto& tab = fcs::table();
    EXPECT_EQ(tab.size(), 256u);
    for (auto v : tab) EXPECT_LE(v, 0xFFFF);
}

TEST(Fcs, GoodFcsResidual) {
    std::vector<Bytes> payloads = {
        {}, {0x00}, {0xFF, 0x03, 0x01, 0x02, 0x03}, {}};
    Bytes seq64;
    for (int i = 0; i < 64; ++i) seq64.push_back(static_cast<std::uint8_t>(i));
    payloads.back() = seq64;

    for (const auto& payload : payloads) {
        Bytes framed = payload;
        Bytes f = frame_fcs(payload);
        framed.insert(framed.end(), f.begin(), f.end());
        EXPECT_EQ(fcs16(framed), fcs::GOOD_FCS16);
        EXPECT_TRUE(check_fcs(framed));
    }
}

TEST(Fcs, CheckFcsRejectsCorruption) {
    Bytes payload = {0xFF, 0x03, 'h', 'e', 'l', 'l', 'o'};
    Bytes framed = payload;
    Bytes f = frame_fcs(payload);
    framed.insert(framed.end(), f.begin(), f.end());
    framed[3] ^= 0x01;  // flip a bit
    EXPECT_FALSE(check_fcs(framed));
}

TEST(Fcs, FrameFcsShapeAndDeterminism) {
    Bytes out = frame_fcs({0xFF, 0x03, 0x21});
    EXPECT_EQ(out.size(), 2u);
    EXPECT_EQ(out, frame_fcs(Bytes{0xFF, 0x03, 0x21}));
}
