#include <gtest/gtest.h>
#include <coco/Buffer.hpp>
#include <coco/BufferReader.hpp>
#include <coco/BufferWriter.hpp>
#include <coco/ArrayConcept.hpp>
#include <coco/StreamOperators.hpp>
#include <coco/ip.hpp>


using namespace coco;

TEST(cocoTest, Net16) {
    ip::Net16 x = 80;
    ip::Net16 y = 8080;

    EXPECT_EQ(x.value, 80 << 8);
    EXPECT_EQ(x, 80);
    EXPECT_TRUE(x < y);
}

TEST(cocoTest, Net32) {
    ip::Net32 x = 0x7f000001;
    ip::Net32 y = 0x7f001001;

    EXPECT_EQ(x.value, 0x0100007f);
    EXPECT_EQ(x, 0x7f000001);
    EXPECT_TRUE(x < y);
}

TEST(cocoTest, ipv4Address) {
    ip::v4::Address a4 = *ip::v4::Address::fromString("127.0.0.1");
    EXPECT_EQ(a4.u8[0], 127);
    EXPECT_EQ(a4.u8[1], 0);
    EXPECT_EQ(a4.u8[2], 0);
    EXPECT_EQ(a4.u8[3], 1);
    EXPECT_EQ(a4.u32[0], 0x7f000001);
}

TEST(cocoTest, ipv6Address) {
    ip::v6::Address a6 = *ip::v6::Address::fromString("::1");
    EXPECT_EQ(a6.u8[0], 0);
    EXPECT_EQ(a6.u8[1], 0);
    EXPECT_EQ(a6.u8[14], 0);
    EXPECT_EQ(a6.u8[15], 1);
    EXPECT_EQ(a6.u32[3], 0x00000001);
}

TEST(cocoTest, ipv4Endpoint) {
    ip::Endpoint ep = {.v4 = {.port = 80, .address = *ip::v4::Address::fromString("127.0.0.1")}};

    // protocolId member is initialized automatically
    EXPECT_EQ(ep.protocolId, ip::v4::PROTOCOL_ID);
    EXPECT_EQ(ep.v4.port, 80);
    EXPECT_EQ(ep.v4.address.u32[0], 0x7f000001);
}

TEST(cocoTest, ipv6Endpoint) {
    ip::Endpoint ep = {.v6 = {.port = 80, .address = *ip::v6::Address::fromString("::1")}};

    // protocolId member is initialized automatically
    EXPECT_EQ(ep.protocolId, ip::v6::PROTOCOL_ID);
    EXPECT_EQ(ep.v6.port, 80);
    EXPECT_EQ(ep.v6.address.u32[3], 0x00000001);
}

TEST(cocoTest, ipEndpoint) {
    // has to be initialized because the protocolId members have an initializer
    ip::Endpoint ep = {};

    EXPECT_EQ(ep.protocolId, 0);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    int success = RUN_ALL_TESTS();
    return success;
}
