#include <gtest/gtest.h>
#include "p2p/core/types.hpp"
#include <vector>
#include <string>

namespace {

using namespace p2p;

// Test basic multiaddr parsing
TEST(MultiaddrTest, ParseIPv4TCP) {
    Multiaddr addr("/ip4/127.0.0.1/tcp/4001");
    auto status = addr.Parse();
    EXPECT_TRUE(status.ok()) << status.message();
}

TEST(MultiaddrTest, ParseIPv4UDP) {
    Multiaddr addr("/ip4/192.168.1.1/udp/5000");
    auto status = addr.Parse();
    EXPECT_TRUE(status.ok()) << status.message();
}

TEST(MultiaddrTest, ParseIPv6TCP) {
    Multiaddr addr("/ip6/::1/tcp/8080");
    auto status = addr.Parse();
    EXPECT_TRUE(status.ok()) << status.message();
}

TEST(MultiaddrTest, ParseWithPeerID) {
    Multiaddr addr("/ip4/127.0.0.1/tcp/4001/p2p/QmYyQSo1c1Ym7orWxLYvCrM2EmxFTANf8wXmmE7DWjhx5N");
    auto status = addr.Parse();
    EXPECT_TRUE(status.ok()) << status.message();
}

// Test serialization to bytes
TEST(MultiaddrTest, ToBytes) {
    Multiaddr addr("/ip4/127.0.0.1/tcp/4001");
    auto status = addr.Parse();
    ASSERT_TRUE(status.ok());

    auto bytes = addr.ToBytes();
    EXPECT_FALSE(bytes.empty());

    // Expected: varint(4) + 127.0.0.1 + varint(6) + varint(4001)
    // ip4 = 4, tcp = 6
    // 127.0.0.1 = 0x7F 0x00 0x00 0x01
    // 4001 = 0x0FA1 (big-endian: 0x0F 0xA1)
    std::vector<uint8_t> expected = {
        0x04,                    // ip4 protocol code
        0x7F, 0x00, 0x00, 0x01,  // 127.0.0.1
        0x06,                    // tcp protocol code
        0x0F, 0xA1               // port 4001 (big-endian)
    };
    EXPECT_EQ(bytes, expected);
}

// Test deserialization from bytes
TEST(MultiaddrTest, FromBytes) {
    std::vector<uint8_t> bytes = {
        0x04,                    // ip4
        0x7F, 0x00, 0x00, 0x01,  // 127.0.0.1
        0x06,                    // tcp
        0x0F, 0xA1               // port 4001
    };

    auto result = Multiaddr::FromBytes(bytes);
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->ToString(), "/ip4/127.0.0.1/tcp/4001");
}

// Test round-trip conversion
TEST(MultiaddrTest, RoundTrip) {
    std::vector<std::string> test_addrs = {
        "/ip4/127.0.0.1/tcp/4001",
        "/ip4/192.168.1.1/udp/5000",
        "/ip6/::1/tcp/8080",
        "/ip4/10.0.0.1/tcp/9000/p2p/QmYyQSo1c1Ym7orWxLYvCrM2EmxFTANf8wXmmE7DWjhx5N"
    };

    for (const auto& addr_str : test_addrs) {
        Multiaddr addr(addr_str);
        auto status = addr.Parse();
        ASSERT_TRUE(status.ok()) << "Failed to parse: " << addr_str;

        auto bytes = addr.ToBytes();
        ASSERT_FALSE(bytes.empty());

        auto result = Multiaddr::FromBytes(bytes);
        ASSERT_TRUE(result.has_value()) << "Failed to deserialize: " << addr_str;

        EXPECT_EQ(result->ToString(), addr_str);
    }
}

// Test error cases
TEST(MultiaddrTest, ParseErrors) {
    // Invalid protocol
    Multiaddr addr1("/invalid/127.0.0.1");
    EXPECT_FALSE(addr1.Parse().ok());

    // Missing address
    Multiaddr addr2("/ip4/tcp/4001");
    EXPECT_FALSE(addr2.Parse().ok());

    // Invalid IP address
    Multiaddr addr3("/ip4/999.999.999.999/tcp/4001");
    EXPECT_FALSE(addr3.Parse().ok());

    // Invalid port
    Multiaddr addr4("/ip4/127.0.0.1/tcp/99999");
    EXPECT_FALSE(addr4.Parse().ok());
}

}  // namespace
