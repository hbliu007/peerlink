#include <gtest/gtest.h>
#include "p2p/nat/stun_client.hpp"
#include <boost/asio.hpp>

using namespace p2p::nat;

class STUNClientTest : public ::testing::Test {
protected:
    std::unique_ptr<boost::asio::io_context> io_context_;

    void SetUp() override {
        io_context_ = std::make_unique<boost::asio::io_context>();
    }

    void TearDown() override {
        if (io_context_) {
            io_context_->stop();
        }
    }
};

TEST_F(STUNClientTest, Construction) {
    STUNClient client(*io_context_, "stun.l.google.com", 19302);
    EXPECT_NE(&client, nullptr);
}

TEST_F(STUNClientTest, PackSTUNRequest) {
    STUNClient client(*io_context_, "stun.l.google.com", 19302);

    auto request = client.packStunRequest();

    // STUN request should be exactly 20 bytes (header only, no attributes)
    EXPECT_EQ(request.size(), 20);

    // Check message type (BINDING_REQUEST = 0x0001)
    EXPECT_EQ(request[0], 0x00);
    EXPECT_EQ(request[1], 0x01);

    // Check message length (0 for no attributes)
    EXPECT_EQ(request[2], 0x00);
    EXPECT_EQ(request[3], 0x00);

    // Check magic cookie
    EXPECT_EQ(request[4], 0x21);
    EXPECT_EQ(request[5], 0x12);
    EXPECT_EQ(request[6], 0xA4);
    EXPECT_EQ(request[7], 0x42);
}

TEST_F(STUNClientTest, UnpackInvalidResponse) {
    STUNClient client(*io_context_, "stun.l.google.com", 19302);

    // Too short
    std::vector<uint8_t> short_data = {1, 2, 3};
    auto [ip1, port1] = client.unpackStunResponse(short_data);
    EXPECT_FALSE(ip1.has_value());
    EXPECT_FALSE(port1.has_value());

    // Invalid magic cookie
    std::vector<uint8_t> invalid_magic(20, 0);
    auto [ip2, port2] = client.unpackStunResponse(invalid_magic);
    EXPECT_FALSE(ip2.has_value());
    EXPECT_FALSE(port2.has_value());
}

TEST_F(STUNClientTest, UnpackXORMappedAddress) {
    STUNClient client(*io_context_, "stun.l.google.com", 19302);

    // Create a mock STUN response with XOR-MAPPED-ADDRESS
    std::vector<uint8_t> response;

    // Message type (BINDING_RESPONSE = 0x0101)
    response.push_back(0x01);
    response.push_back(0x01);

    // Message length (12 bytes for one attribute)
    response.push_back(0x00);
    response.push_back(0x0C);

    // Magic cookie
    response.push_back(0x21);
    response.push_back(0x12);
    response.push_back(0xA4);
    response.push_back(0x42);

    // Transaction ID (12 bytes)
    for (int i = 0; i < 12; ++i) {
        response.push_back(static_cast<uint8_t>(i));
    }

    // XOR-MAPPED-ADDRESS attribute
    // Type (0x0020)
    response.push_back(0x00);
    response.push_back(0x20);

    // Length (8 bytes)
    response.push_back(0x00);
    response.push_back(0x08);

    // Reserved + Family (IPv4 = 0x01)
    response.push_back(0x00);
    response.push_back(0x01);

    // XOR Port (test port 12345 XOR 0x2112 = 0x3039)
    uint16_t test_port = 12345;
    uint16_t xor_port = test_port ^ 0x2112;
    response.push_back((xor_port >> 8) & 0xFF);
    response.push_back(xor_port & 0xFF);

    // XOR IP (192.168.1.100 XOR magic cookie)
    uint32_t test_ip = (192 << 24) | (168 << 16) | (1 << 8) | 100;
    uint32_t xor_ip = test_ip ^ 0x2112A442;
    response.push_back((xor_ip >> 24) & 0xFF);
    response.push_back((xor_ip >> 16) & 0xFF);
    response.push_back((xor_ip >> 8) & 0xFF);
    response.push_back(xor_ip & 0xFF);

    auto [ip, port] = client.unpackStunResponse(response);

    ASSERT_TRUE(ip.has_value());
    ASSERT_TRUE(port.has_value());
    EXPECT_EQ(ip.value(), "192.168.1.100");
    EXPECT_EQ(port.value(), 12345);
}

TEST_F(STUNClientTest, UnpackMappedAddress) {
    STUNClient client(*io_context_, "stun.l.google.com", 19302);

    // Create a mock STUN response with MAPPED-ADDRESS (not XOR)
    std::vector<uint8_t> response;

    // Message type
    response.push_back(0x01);
    response.push_back(0x01);

    // Message length
    response.push_back(0x00);
    response.push_back(0x0C);

    // Magic cookie
    response.push_back(0x21);
    response.push_back(0x12);
    response.push_back(0xA4);
    response.push_back(0x42);

    // Transaction ID
    for (int i = 0; i < 12; ++i) {
        response.push_back(static_cast<uint8_t>(i));
    }

    // MAPPED-ADDRESS attribute
    // Type (0x0001)
    response.push_back(0x00);
    response.push_back(0x01);

    // Length
    response.push_back(0x00);
    response.push_back(0x08);

    // Reserved + Family
    response.push_back(0x00);
    response.push_back(0x01);

    // Port (54321)
    response.push_back((54321 >> 8) & 0xFF);
    response.push_back(54321 & 0xFF);

    // IP (203.0.113.1)
    response.push_back(203);
    response.push_back(0);
    response.push_back(113);
    response.push_back(1);

    auto [ip, port] = client.unpackStunResponse(response);

    ASSERT_TRUE(ip.has_value());
    ASSERT_TRUE(port.has_value());
    EXPECT_EQ(ip.value(), "203.0.113.1");
    EXPECT_EQ(port.value(), 54321);
}

TEST_F(STUNClientTest, NATTypeDetection) {
    // Test NAT type classification
    EXPECT_TRUE(is_nat_p2p_capable(NATType::PUBLIC_IP));
    EXPECT_TRUE(is_nat_p2p_capable(NATType::FULL_CONE));
    EXPECT_TRUE(is_nat_p2p_capable(NATType::RESTRICTED_CONE));
    EXPECT_TRUE(is_nat_p2p_capable(NATType::PORT_RESTRICTED_CONE));

    EXPECT_FALSE(is_nat_p2p_capable(NATType::SYMMETRIC));
    EXPECT_FALSE(is_nat_p2p_capable(NATType::BLOCKED));
}

TEST_F(STUNClientTest, MultipleRequests) {
    STUNClient client(*io_context_, "stun.l.google.com", 19302);

    // Generate multiple requests
    auto request1 = client.packStunRequest();
    auto request2 = client.packStunRequest();

    // Requests should have different transaction IDs
    bool different = false;
    for (size_t i = 8; i < 20; ++i) {
        if (request1[i] != request2[i]) {
            different = true;
            break;
        }
    }

    EXPECT_TRUE(different);
}

TEST_F(STUNClientTest, ResponseWithPadding) {
    STUNClient client(*io_context_, "stun.l.google.com", 19302);

    // Create response with attribute that needs padding
    std::vector<uint8_t> response;

    // Header
    response.push_back(0x01);
    response.push_back(0x01);
    response.push_back(0x00);
    response.push_back(0x10);  // 16 bytes (12 for attr + 4 padding)

    // Magic cookie
    response.push_back(0x21);
    response.push_back(0x12);
    response.push_back(0xA4);
    response.push_back(0x42);

    // Transaction ID
    for (int i = 0; i < 12; ++i) {
        response.push_back(static_cast<uint8_t>(i));
    }

    // XOR-MAPPED-ADDRESS
    response.push_back(0x00);
    response.push_back(0x20);
    response.push_back(0x00);
    response.push_back(0x08);

    response.push_back(0x00);
    response.push_back(0x01);

    uint16_t xor_port = 8080 ^ 0x2112;
    response.push_back((xor_port >> 8) & 0xFF);
    response.push_back(xor_port & 0xFF);

    uint32_t test_ip = (10 << 24) | (0 << 16) | (0 << 8) | 1;
    uint32_t xor_ip = test_ip ^ 0x2112A442;
    response.push_back((xor_ip >> 24) & 0xFF);
    response.push_back((xor_ip >> 16) & 0xFF);
    response.push_back((xor_ip >> 8) & 0xFF);
    response.push_back(xor_ip & 0xFF);

    // Padding (4 bytes to align to 4-byte boundary)
    response.push_back(0);
    response.push_back(0);
    response.push_back(0);
    response.push_back(0);

    auto [ip, port] = client.unpackStunResponse(response);

    ASSERT_TRUE(ip.has_value());
    ASSERT_TRUE(port.has_value());
    EXPECT_EQ(ip.value(), "10.0.0.1");
    EXPECT_EQ(port.value(), 8080);
}

TEST_F(STUNClientTest, TruncatedAttribute) {
    STUNClient client(*io_context_, "stun.l.google.com", 19302);

    // Create response with truncated attribute
    std::vector<uint8_t> response;

    // Header
    response.push_back(0x01);
    response.push_back(0x01);
    response.push_back(0x00);
    response.push_back(0x0C);

    // Magic cookie
    response.push_back(0x21);
    response.push_back(0x12);
    response.push_back(0xA4);
    response.push_back(0x42);

    // Transaction ID
    for (int i = 0; i < 12; ++i) {
        response.push_back(static_cast<uint8_t>(i));
    }

    // Attribute header claims 8 bytes but data is truncated
    response.push_back(0x00);
    response.push_back(0x20);
    response.push_back(0x00);
    response.push_back(0x08);

    // Only 2 bytes of data (should be 8)
    response.push_back(0x00);
    response.push_back(0x01);

    auto [ip, port] = client.unpackStunResponse(response);

    // Should fail gracefully
    EXPECT_FALSE(ip.has_value());
    EXPECT_FALSE(port.has_value());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
