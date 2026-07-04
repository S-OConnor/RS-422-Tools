// Mirrors tests/test_register_codec.py — register message round-trips,
// variable-length ReadResponse, malformed-length rejection, Nak reason text.
#include <gtest/gtest.h>

#include "serial_link/codec.hpp"

using namespace serial_link;
using namespace serial_link::codec;

namespace {
template <class M>
void expect_roundtrip(const M& msg) {
    Message back = decode(msg.encode());
    ASSERT_TRUE(std::holds_alternative<M>(back));
    EXPECT_TRUE(std::get<M>(back) == msg);
}
}  // namespace

TEST(RegisterCodec, MessagesRoundtrip) {
    expect_roundtrip(ReadRegister{0x0010, 4});
    expect_roundtrip(WriteRegister{0x0020, 0x1234});
    expect_roundtrip(WriteAck{0x0020, 0x1234, 0});
    expect_roundtrip(Nak{2, 0x0020});
    expect_roundtrip(ReadResponse{0x10, {1, 2, 3}});
    expect_roundtrip(ReadResponse{0x10, {0xBEEF}});
    expect_roundtrip(ReadResponse{0x10, {}});  // empty array
}

TEST(RegisterCodec, TypeIdsRegistered) {
    EXPECT_EQ(type_id_of(Message{ReadRegister{}}), 0x01);
    EXPECT_EQ(type_id_of(Message{WriteRegister{}}), 0x02);
    EXPECT_EQ(type_id_of(Message{ReadResponse{}}), 0x81);
    EXPECT_EQ(type_id_of(Message{WriteAck{}}), 0x82);
    EXPECT_EQ(type_id_of(Message{Nak{}}), 0xFF);
}

TEST(RegisterCodec, ReadResponseVariableLengthSize) {
    Bytes info = ReadResponse{0x10, {0xAAAA, 0xBBBB, 0xCCCC}}.encode();
    // type_id(1) + addr(2) + 3 * u16(2) = 9
    EXPECT_EQ(info.size(), 1u + 2u + 3u * 2u);
}

TEST(RegisterCodec, ReadResponseBadTrailingLengthRaises) {
    Bytes bad = {ReadResponse::TYPE_ID, 0x00, 0x10, 0xAA};  // odd trailing byte
    EXPECT_THROW(decode(bad), CodecError);
}

TEST(RegisterCodec, EmptyInfoRaises) {
    EXPECT_THROW(decode(Bytes{}), CodecError);
}

TEST(RegisterCodec, UnknownTypeIdRaises) {
    EXPECT_THROW(decode(Bytes{0x7A, 0x00}), UnknownMessage);
}

TEST(RegisterCodec, NakReasonText) {
    EXPECT_EQ((Nak{2, 0}).reason(), "bad address");
}
