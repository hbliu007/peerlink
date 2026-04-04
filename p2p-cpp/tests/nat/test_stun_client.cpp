// TESTING defined by CMake for this target
#include "p2p/nat/stun_client.hpp"
#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <thread>
#include <chrono>

using namespace p2p::nat;

class STUNClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        io_context_ = std::make_unique<boost::asio::io_context>();
    }

    void TearDown() override {
        io_context_->stop();
        if (io_thread_.joinable()) {
            io_thread_.join();
        }
    }

    void RunIOContext() {
        io_thread_ = std::thread([this]() {
            io_context_->run();
        });
    }

    std::unique_ptr<boost::asio::io_context> io_context_;
    std::thread io_thread_;
};

/**
 * Test STUN request packet generation
 */
TEST_F(STUNClientTest, PackSTUNRequest_ValidFormat) {
    STUNClient client(*io_context_, "stun.l.google.com", 19302);

    auto request = client.packStunRequest();

    // Verify packet structure
    ASSERT_GE(request.size(), 20u);  // Minimum STUN message size

    // Verify message type (BINDING_REQUEST = 0x0001)
    EXPECT_EQ(request[0], 0x00);
    EXPECT_EQ(request[1], 0x01);

    // Verify message length (should be 0 for no attributes)
    EXPECT_EQ(request[2], 0x00);
    EXPECT_EQ(request[3], 0x00);

    // Verify magic cookie (0x2112A442)
    EXPECT_EQ(request[4], 0x21);
    EXPECT_EQ(request[5], 0x12);
    EXPECT_EQ(request[6], 0xA4);
    EXPECT_EQ(request[7], 0x42);

    // Verify transaction ID is 12 bytes
    EXPECT_EQ(request.size(), 20u);
}

/**
 * Test STUN request uniqueness (transaction IDs should differ)
 */
TEST_F(STUNClientTest, PackSTUNRequest_UniqueTransactionIDs) {
    STUNClient client(*io_context_, "stun.l.google.com", 19302);

    auto request1 = client.packStunRequest();
    auto request2 = client.packStunRequest();

    // Transaction IDs start at byte 8
    bool transaction_ids_differ = false;
    for (size_t i = 8; i < 20; ++i) {
        if (request1[i] != request2[i]) {
            transaction_ids_differ = true;
            break;
        }
    }

    EXPECT_TRUE(transaction_ids_differ)
        << "Transaction IDs should be unique";
}

/**
 * Test unpacking valid STUN response with XOR-MAPPED-ADDRESS
 */
TEST_F(STUNClientTest, UnpackSTUNResponse_XORMappedAddress) {
    STUNClient client(*io_context_, "stun.l.google.com", 19302);

    // Craft a valid STUN response with XOR-MAPPED-ADDRESS
    // Response for IP 192.168.1.100, port 54321
    std::vector<uint8_t> response = {
        // Message type: BINDING_RESPONSE (0x0101)
        0x01, 0x01,
        // Message length: 12 bytes (one attribute)
        0x00, 0x0C,
        // Magic cookie
        0x21, 0x12, 0xA4, 0x42,
        // Transaction ID (12 bytes)
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
        0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
        // Attribute: XOR-MAPPED-ADDRESS (0x0020)
        0x00, 0x20,
        // Attribute length: 8 bytes
        0x00, 0x08,
        // Reserved (1 byte) + Family (1 byte, 0x01 = IPv4)
        0x00, 0x01,
        // XOR'd port: 54321 ^ 0x2112 = 0xD431 ^ 0x2112 = 0xF523
        0xF5, 0x23,
        // XOR'd IP: 192.168.1.100 ^ 0x2112A442
        // 192.168.1.100 = 0xC0A80164
        // 0xC0A80164 ^ 0x2112A442 = 0xE1BAA526
        0xE1, 0xBA, 0xA5, 0x26
    };

    auto [ip, port] = client.unpackStunResponse(response);

    ASSERT_TRUE(ip.has_value());
    ASSERT_TRUE(port.has_value());
    EXPECT_EQ(*ip, "192.168.1.100");
    EXPECT_EQ(*port, 54321);
}

/**
 * Test unpacking STUN response with MAPPED-ADDRESS (fallback)
 */
TEST_F(STUNClientTest, UnpackSTUNResponse_MappedAddress) {
    STUNClient client(*io_context_, "stun.l.google.com", 19302);

    // Craft a valid STUN response with MAPPED-ADDRESS
    // Response for IP 10.0.0.1, port 12345
    std::vector<uint8_t> response = {
        // Message type: BINDING_RESPONSE (0x0101)
        0x01, 0x01,
        // Message length: 12 bytes
        0x00, 0x0C,
        // Magic cookie
        0x21, 0x12, 0xA4, 0x42,
        // Transaction ID
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
        0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
        // Attribute: MAPPED-ADDRESS (0x0001)
        0x00, 0x01,
        // Attribute length: 8 bytes
        0x00, 0x08,
        // Reserved + Family (IPv4)
        0x00, 0x01,
        // Port: 12345 = 0x3039
        0x30, 0x39,
        // IP: 10.0.0.1 = 0x0A000001
        0x0A, 0x00, 0x00, 0x01
    };

    auto [ip, port] = client.unpackStunResponse(response);

    ASSERT_TRUE(ip.has_value());
    ASSERT_TRUE(port.has_value());
    EXPECT_EQ(*ip, "10.0.0.1");
    EXPECT_EQ(*port, 12345);
}

/**
 * Test unpacking response that's too short
 */
TEST_F(STUNClientTest, UnpackSTUNResponse_TooShort) {
    STUNClient client(*io_context_, "stun.l.google.com", 19302);

    std::vector<uint8_t> response = {0x01, 0x01, 0x00, 0x00};  // Only 4 bytes

    auto [ip, port] = client.unpackStunResponse(response);

    EXPECT_FALSE(ip.has_value());
    EXPECT_FALSE(port.has_value());
}

/**
 * Test unpacking response with invalid magic cookie
 */
TEST_F(STUNClientTest, UnpackSTUNResponse_InvalidMagicCookie) {
    STUNClient client(*io_context_, "stun.l.google.com", 19302);

    std::vector<uint8_t> response = {
        0x01, 0x01, 0x00, 0x00,
        // Wrong magic cookie
        0xFF, 0xFF, 0xFF, 0xFF,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
        0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C
    };

    auto [ip, port] = client.unpackStunResponse(response);

    EXPECT_FALSE(ip.has_value());
    EXPECT_FALSE(port.has_value());
}

/**
 * Test unpacking response with no address attributes
 */
TEST_F(STUNClientTest, UnpackSTUNResponse_NoAddressAttribute) {
    STUNClient client(*io_context_, "stun.l.google.com", 19302);

    std::vector<uint8_t> response = {
        0x01, 0x01, 0x00, 0x00,
        0x21, 0x12, 0xA4, 0x42,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
        0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C
    };

    auto [ip, port] = client.unpackStunResponse(response);

    EXPECT_FALSE(ip.has_value());
    EXPECT_FALSE(port.has_value());
}

/**
 * Test NAT type detection helper function
 */
TEST_F(STUNClientTest, IsNATP2PCapable_PublicIP) {
    EXPECT_TRUE(is_nat_p2p_capable(NATType::PUBLIC_IP));
}

TEST_F(STUNClientTest, IsNATP2PCapable_FullCone) {
    EXPECT_TRUE(is_nat_p2p_capable(NATType::FULL_CONE));
}

TEST_F(STUNClientTest, IsNATP2PCapable_RestrictedCone) {
    EXPECT_TRUE(is_nat_p2p_capable(NATType::RESTRICTED_CONE));
}

TEST_F(STUNClientTest, IsNATP2PCapable_PortRestrictedCone) {
    EXPECT_TRUE(is_nat_p2p_capable(NATType::PORT_RESTRICTED_CONE));
}

TEST_F(STUNClientTest, IsNATP2PCapable_Symmetric) {
    EXPECT_FALSE(is_nat_p2p_capable(NATType::SYMMETRIC));
}

TEST_F(STUNClientTest, IsNATP2PCapable_Unknown) {
    EXPECT_FALSE(is_nat_p2p_capable(NATType::UNKNOWN));
}

TEST_F(STUNClientTest, IsNATP2PCapable_Blocked) {
    EXPECT_FALSE(is_nat_p2p_capable(NATType::BLOCKED));
}

/**
 * Test unpacking response with truncated attribute
 */
TEST_F(STUNClientTest, UnpackSTUNResponse_TruncatedAttribute) {
    STUNClient client(*io_context_, "stun.l.google.com", 19302);

    std::vector<uint8_t> response = {
        0x01, 0x01,
        0x00, 0x0C,  // Claims 12 bytes of attributes
        0x21, 0x12, 0xA4, 0x42,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
        0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
        // Attribute header but no data
        0x00, 0x20,
        0x00, 0x08
        // Missing 8 bytes of attribute data
    };

    auto [ip, port] = client.unpackStunResponse(response);

    // Should handle gracefully
    EXPECT_FALSE(ip.has_value());
    EXPECT_FALSE(port.has_value());
}

/**
 * Test unpacking response with attribute padding
 */
TEST_F(STUNClientTest, UnpackSTUNResponse_AttributePadding) {
    STUNClient client(*io_context_, "stun.l.google.com", 19302);

    // Response with an attribute that needs padding
    std::vector<uint8_t> response = {
        0x01, 0x01,
        0x00, 0x10,  // 16 bytes of attributes (12 + 4 padding)
        0x21, 0x12, 0xA4, 0x42,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
        0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
        // XOR-MAPPED-ADDRESS
        0x00, 0x20,
        0x00, 0x08,
        0x00, 0x01,
        0xF5, 0x23,  // Port
        0xE1, 0xBA, 0xA5, 0x26  // IP
    };

    auto [ip, port] = client.unpackStunResponse(response);

    ASSERT_TRUE(ip.has_value());
    ASSERT_TRUE(port.has_value());
    EXPECT_EQ(*ip, "192.168.1.100");
    EXPECT_EQ(*port, 54321);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
