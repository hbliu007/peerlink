#include "p2p/nat/multiaddr_converter.hpp"
#include <gtest/gtest.h>
#include <arpa/inet.h>

using namespace p2p::nat;
using namespace p2p::net;

class MultiaddrConverterTest : public ::testing::Test {
protected:
    // Helper: Encode varint (unsigned LEB128)
    std::vector<uint8_t> EncodeVarint(uint64_t value) {
        std::vector<uint8_t> result;
        while (value >= 0x80) {
            result.push_back(static_cast<uint8_t>((value & 0x7F) | 0x80));
            value >>= 7;
        }
        result.push_back(static_cast<uint8_t>(value & 0x7F));
        return result;
    }

    // Helper: Create IPv4/TCP multiaddr
    std::vector<uint8_t> CreateIPv4TCPMultiaddr(const std::string& ip, uint16_t port) {
        std::vector<uint8_t> result;

        // IP4 protocol code (4)
        auto ip4_code = EncodeVarint(4);
        result.insert(result.end(), ip4_code.begin(), ip4_code.end());

        // IPv4 address (4 bytes)
        uint8_t ip_bytes[4];
        inet_pton(AF_INET, ip.c_str(), ip_bytes);
        result.insert(result.end(), ip_bytes, ip_bytes + 4);

        // TCP protocol code (6)
        auto tcp_code = EncodeVarint(6);
        result.insert(result.end(), tcp_code.begin(), tcp_code.end());

        // Port (2 bytes, big-endian)
        result.push_back(static_cast<uint8_t>(port >> 8));
        result.push_back(static_cast<uint8_t>(port & 0xFF));

        return result;
    }

    // Helper: Create IPv4/UDP multiaddr
    std::vector<uint8_t> CreateIPv4UDPMultiaddr(const std::string& ip, uint16_t port) {
        std::vector<uint8_t> result;

        // IP4 protocol code (4)
        auto ip4_code = EncodeVarint(4);
        result.insert(result.end(), ip4_code.begin(), ip4_code.end());

        // IPv4 address (4 bytes)
        uint8_t ip_bytes[4];
        inet_pton(AF_INET, ip.c_str(), ip_bytes);
        result.insert(result.end(), ip_bytes, ip_bytes + 4);

        // UDP protocol code (273)
        auto udp_code = EncodeVarint(273);
        result.insert(result.end(), udp_code.begin(), udp_code.end());

        // Port (2 bytes, big-endian)
        result.push_back(static_cast<uint8_t>(port >> 8));
        result.push_back(static_cast<uint8_t>(port & 0xFF));

        return result;
    }

    // Helper: Create IPv6/TCP multiaddr
    std::vector<uint8_t> CreateIPv6TCPMultiaddr(const std::string& ip, uint16_t port) {
        std::vector<uint8_t> result;

        // IP6 protocol code (41)
        auto ip6_code = EncodeVarint(41);
        result.insert(result.end(), ip6_code.begin(), ip6_code.end());

        // IPv6 address (16 bytes)
        uint8_t ip_bytes[16];
        inet_pton(AF_INET6, ip.c_str(), ip_bytes);
        result.insert(result.end(), ip_bytes, ip_bytes + 16);

        // TCP protocol code (6)
        auto tcp_code = EncodeVarint(6);
        result.insert(result.end(), tcp_code.begin(), tcp_code.end());

        // Port (2 bytes, big-endian)
        result.push_back(static_cast<uint8_t>(port >> 8));
        result.push_back(static_cast<uint8_t>(port & 0xFF));

        return result;
    }

    // Helper: Create IPv6/UDP multiaddr
    std::vector<uint8_t> CreateIPv6UDPMultiaddr(const std::string& ip, uint16_t port) {
        std::vector<uint8_t> result;

        // IP6 protocol code (41)
        auto ip6_code = EncodeVarint(41);
        result.insert(result.end(), ip6_code.begin(), ip6_code.end());

        // IPv6 address (16 bytes)
        uint8_t ip_bytes[16];
        inet_pton(AF_INET6, ip.c_str(), ip_bytes);
        result.insert(result.end(), ip_bytes, ip_bytes + 16);

        // UDP protocol code (273)
        auto udp_code = EncodeVarint(273);
        result.insert(result.end(), udp_code.begin(), udp_code.end());

        // Port (2 bytes, big-endian)
        result.push_back(static_cast<uint8_t>(port >> 8));
        result.push_back(static_cast<uint8_t>(port & 0xFF));

        return result;
    }
};

// --- parse_tcp Tests ---

TEST_F(MultiaddrConverterTest, ParseTCP_IPv4_ValidAddress) {
    auto multiaddr = CreateIPv4TCPMultiaddr("127.0.0.1", 8080);
    auto result = MultiaddrConverter::parse_tcp(multiaddr);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->ip, "127.0.0.1");
    EXPECT_EQ(result->port, 8080);
}

TEST_F(MultiaddrConverterTest, ParseTCP_IPv4_DifferentAddresses) {
    // Test various IPv4 addresses
    std::vector<std::pair<std::string, uint16_t>> test_cases = {
        {"192.168.1.1", 80},
        {"10.0.0.1", 443},
        {"172.16.0.1", 3000},
        {"0.0.0.0", 0},
        {"255.255.255.255", 65535}
    };

    for (const auto& [ip, port] : test_cases) {
        auto multiaddr = CreateIPv4TCPMultiaddr(ip, port);
        auto result = MultiaddrConverter::parse_tcp(multiaddr);

        ASSERT_TRUE(result.has_value()) << "Failed for " << ip << ":" << port;
        EXPECT_EQ(result->ip, ip);
        EXPECT_EQ(result->port, port);
    }
}

TEST_F(MultiaddrConverterTest, ParseTCP_IPv6_ValidAddress) {
    auto multiaddr = CreateIPv6TCPMultiaddr("::1", 8080);
    auto result = MultiaddrConverter::parse_tcp(multiaddr);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->ip, "::1");
    EXPECT_EQ(result->port, 8080);
}

TEST_F(MultiaddrConverterTest, ParseTCP_IPv6_FullAddress) {
    auto multiaddr = CreateIPv6TCPMultiaddr("fe80::1", 443);
    auto result = MultiaddrConverter::parse_tcp(multiaddr);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->ip, "fe80::1");
    EXPECT_EQ(result->port, 443);
}

TEST_F(MultiaddrConverterTest, ParseTCP_EmptyData_ReturnsNullopt) {
    std::vector<uint8_t> empty_data;
    auto result = MultiaddrConverter::parse_tcp(empty_data);
    EXPECT_FALSE(result.has_value());
}

TEST_F(MultiaddrConverterTest, ParseTCP_UDPAddress_ReturnsNullopt) {
    // Create UDP multiaddr, but try to parse as TCP
    auto multiaddr = CreateIPv4UDPMultiaddr("127.0.0.1", 5000);
    auto result = MultiaddrConverter::parse_tcp(multiaddr);
    EXPECT_FALSE(result.has_value());
}

TEST_F(MultiaddrConverterTest, ParseTCP_IncompleteIP_ReturnsNullopt) {
    std::vector<uint8_t> incomplete;
    // IP4 protocol code
    auto ip4_code = EncodeVarint(4);
    incomplete.insert(incomplete.end(), ip4_code.begin(), ip4_code.end());
    // Only 2 bytes of IPv4 address (should be 4)
    incomplete.push_back(0x7F);
    incomplete.push_back(0x00);

    auto result = MultiaddrConverter::parse_tcp(incomplete);
    EXPECT_FALSE(result.has_value());
}

TEST_F(MultiaddrConverterTest, ParseTCP_IncompletePort_ReturnsNullopt) {
    std::vector<uint8_t> incomplete;
    // IP4 protocol code
    auto ip4_code = EncodeVarint(4);
    incomplete.insert(incomplete.end(), ip4_code.begin(), ip4_code.end());
    // Full IPv4 address
    uint8_t ip_bytes[4] = {127, 0, 0, 1};
    incomplete.insert(incomplete.end(), ip_bytes, ip_bytes + 4);
    // TCP protocol code
    auto tcp_code = EncodeVarint(6);
    incomplete.insert(incomplete.end(), tcp_code.begin(), tcp_code.end());
    // Only 1 byte of port (should be 2)
    incomplete.push_back(0x1F);

    auto result = MultiaddrConverter::parse_tcp(incomplete);
    EXPECT_FALSE(result.has_value());
}

TEST_F(MultiaddrConverterTest, ParseTCP_OnlyIP_ReturnsNullopt) {
    std::vector<uint8_t> only_ip;
    // IP4 protocol code
    auto ip4_code = EncodeVarint(4);
    only_ip.insert(only_ip.end(), ip4_code.begin(), ip4_code.end());
    // Full IPv4 address
    uint8_t ip_bytes[4] = {127, 0, 0, 1};
    only_ip.insert(only_ip.end(), ip_bytes, ip_bytes + 4);
    // No TCP/port data

    auto result = MultiaddrConverter::parse_tcp(only_ip);
    EXPECT_FALSE(result.has_value());
}

// --- parse_udp Tests ---

TEST_F(MultiaddrConverterTest, ParseUDP_IPv4_ValidAddress) {
    auto multiaddr = CreateIPv4UDPMultiaddr("127.0.0.1", 5000);
    auto result = MultiaddrConverter::parse_udp(multiaddr);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->ip, "127.0.0.1");
    EXPECT_EQ(result->port, 5000);
}

TEST_F(MultiaddrConverterTest, ParseUDP_IPv4_DifferentAddresses) {
    std::vector<std::pair<std::string, uint16_t>> test_cases = {
        {"192.168.1.100", 9000},
        {"10.0.0.50", 12345},
        {"172.16.0.200", 53},
        {"0.0.0.0", 1},
        {"255.255.255.255", 65534}
    };

    for (const auto& [ip, port] : test_cases) {
        auto multiaddr = CreateIPv4UDPMultiaddr(ip, port);
        auto result = MultiaddrConverter::parse_udp(multiaddr);

        ASSERT_TRUE(result.has_value()) << "Failed for " << ip << ":" << port;
        EXPECT_EQ(result->ip, ip);
        EXPECT_EQ(result->port, port);
    }
}

TEST_F(MultiaddrConverterTest, ParseUDP_IPv6_ValidAddress) {
    auto multiaddr = CreateIPv6UDPMultiaddr("::1", 9999);
    auto result = MultiaddrConverter::parse_udp(multiaddr);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->ip, "::1");
    EXPECT_EQ(result->port, 9999);
}

TEST_F(MultiaddrConverterTest, ParseUDP_IPv6_FullAddress) {
    auto multiaddr = CreateIPv6UDPMultiaddr("fe80::1", 53);
    auto result = MultiaddrConverter::parse_udp(multiaddr);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->ip, "fe80::1");
    EXPECT_EQ(result->port, 53);
}

TEST_F(MultiaddrConverterTest, ParseUDP_EmptyData_ReturnsNullopt) {
    std::vector<uint8_t> empty_data;
    auto result = MultiaddrConverter::parse_udp(empty_data);
    EXPECT_FALSE(result.has_value());
}

TEST_F(MultiaddrConverterTest, ParseUDP_TCPAddress_ReturnsNullopt) {
    // Create TCP multiaddr, but try to parse as UDP
    auto multiaddr = CreateIPv4TCPMultiaddr("127.0.0.1", 8080);
    auto result = MultiaddrConverter::parse_udp(multiaddr);
    EXPECT_FALSE(result.has_value());
}

TEST_F(MultiaddrConverterTest, ParseUDP_IncompleteIP_ReturnsNullopt) {
    std::vector<uint8_t> incomplete;
    // IP4 protocol code
    auto ip4_code = EncodeVarint(4);
    incomplete.insert(incomplete.end(), ip4_code.begin(), ip4_code.end());
    // Only 3 bytes of IPv4 address (should be 4)
    incomplete.push_back(0x7F);
    incomplete.push_back(0x00);
    incomplete.push_back(0x00);

    auto result = MultiaddrConverter::parse_udp(incomplete);
    EXPECT_FALSE(result.has_value());
}

TEST_F(MultiaddrConverterTest, ParseUDP_OnlyIP_ReturnsNullopt) {
    std::vector<uint8_t> only_ip;
    // IP4 protocol code
    auto ip4_code = EncodeVarint(4);
    only_ip.insert(only_ip.end(), ip4_code.begin(), ip4_code.end());
    // Full IPv4 address
    uint8_t ip_bytes[4] = {192, 168, 1, 1};
    only_ip.insert(only_ip.end(), ip_bytes, ip_bytes + 4);
    // No UDP/port data

    auto result = MultiaddrConverter::parse_udp(only_ip);
    EXPECT_FALSE(result.has_value());
}

// --- parse General Tests ---

TEST_F(MultiaddrConverterTest, Parse_IPv4TCP_ReturnsCorrectTransport) {
    auto multiaddr = CreateIPv4TCPMultiaddr("127.0.0.1", 8080);
    auto result = MultiaddrConverter::parse(multiaddr);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first, "tcp");
    EXPECT_EQ(result->second.ip, "127.0.0.1");
    EXPECT_EQ(result->second.port, 8080);
}

TEST_F(MultiaddrConverterTest, Parse_IPv4UDP_ReturnsCorrectTransport) {
    auto multiaddr = CreateIPv4UDPMultiaddr("192.168.1.1", 5000);
    auto result = MultiaddrConverter::parse(multiaddr);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first, "udp");
    EXPECT_EQ(result->second.ip, "192.168.1.1");
    EXPECT_EQ(result->second.port, 5000);
}

TEST_F(MultiaddrConverterTest, Parse_IPv6TCP_ReturnsCorrectTransport) {
    auto multiaddr = CreateIPv6TCPMultiaddr("::1", 443);
    auto result = MultiaddrConverter::parse(multiaddr);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first, "tcp");
    EXPECT_EQ(result->second.ip, "::1");
    EXPECT_EQ(result->second.port, 443);
}

TEST_F(MultiaddrConverterTest, Parse_IPv6UDP_ReturnsCorrectTransport) {
    auto multiaddr = CreateIPv6UDPMultiaddr("fe80::1", 9999);
    auto result = MultiaddrConverter::parse(multiaddr);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first, "udp");
    EXPECT_EQ(result->second.ip, "fe80::1");
    EXPECT_EQ(result->second.port, 9999);
}

TEST_F(MultiaddrConverterTest, Parse_EmptyData_ReturnsNullopt) {
    std::vector<uint8_t> empty_data;
    auto result = MultiaddrConverter::parse(empty_data);
    EXPECT_FALSE(result.has_value());
}

TEST_F(MultiaddrConverterTest, Parse_InvalidVarint_ReturnsNullopt) {
    std::vector<uint8_t> invalid_varint;
    // Create a varint that's too long (> 10 bytes)
    for (int i = 0; i < 11; ++i) {
        invalid_varint.push_back(0x80);  // Continuation bit set
    }

    auto result = MultiaddrConverter::parse(invalid_varint);
    EXPECT_FALSE(result.has_value());
}

// --- Boundary Condition Tests ---

TEST_F(MultiaddrConverterTest, Parse_PortZero) {
    auto multiaddr = CreateIPv4TCPMultiaddr("127.0.0.1", 0);
    auto result = MultiaddrConverter::parse_tcp(multiaddr);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->port, 0);
}

TEST_F(MultiaddrConverterTest, Parse_PortMax) {
    auto multiaddr = CreateIPv4TCPMultiaddr("127.0.0.1", 65535);
    auto result = MultiaddrConverter::parse_tcp(multiaddr);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->port, 65535);
}

TEST_F(MultiaddrConverterTest, Parse_IPv4Boundaries) {
    // Test boundary IPv4 addresses
    auto multiaddr1 = CreateIPv4TCPMultiaddr("0.0.0.0", 8080);
    auto result1 = MultiaddrConverter::parse_tcp(multiaddr1);
    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(result1->ip, "0.0.0.0");

    auto multiaddr2 = CreateIPv4TCPMultiaddr("255.255.255.255", 8080);
    auto result2 = MultiaddrConverter::parse_tcp(multiaddr2);
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(result2->ip, "255.255.255.255");
}

TEST_F(MultiaddrConverterTest, Parse_LargeVarint) {
    // Test multi-byte varint encoding
    // UDP protocol code is 273 (0x111), which requires 2 bytes in varint
    auto multiaddr = CreateIPv4UDPMultiaddr("127.0.0.1", 5000);
    auto result = MultiaddrConverter::parse_udp(multiaddr);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->ip, "127.0.0.1");
    EXPECT_EQ(result->port, 5000);
}

// --- Complex Scenario Tests ---

TEST_F(MultiaddrConverterTest, Parse_WithP2PProtocol_SkipsP2P) {
    std::vector<uint8_t> multiaddr;

    // IP4 protocol code (4)
    auto ip4_code = EncodeVarint(4);
    multiaddr.insert(multiaddr.end(), ip4_code.begin(), ip4_code.end());

    // IPv4 address
    uint8_t ip_bytes[4] = {127, 0, 0, 1};
    multiaddr.insert(multiaddr.end(), ip_bytes, ip_bytes + 4);

    // TCP protocol code (6)
    auto tcp_code = EncodeVarint(6);
    multiaddr.insert(multiaddr.end(), tcp_code.begin(), tcp_code.end());

    // Port
    multiaddr.push_back(0x1F);  // 8080 >> 8
    multiaddr.push_back(0x90);  // 8080 & 0xFF

    // P2P protocol code (421)
    auto p2p_code = EncodeVarint(421);
    multiaddr.insert(multiaddr.end(), p2p_code.begin(), p2p_code.end());

    // P2P data length (varint) + data
    auto p2p_len = EncodeVarint(10);
    multiaddr.insert(multiaddr.end(), p2p_len.begin(), p2p_len.end());
    for (int i = 0; i < 10; ++i) {
        multiaddr.push_back(0xAB);
    }

    // Should successfully parse IP/TCP and skip P2P
    auto result = MultiaddrConverter::parse_tcp(multiaddr);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->ip, "127.0.0.1");
    EXPECT_EQ(result->port, 8080);
}

TEST_F(MultiaddrConverterTest, Parse_WithUnknownProtocol_SkipsUnknown) {
    std::vector<uint8_t> multiaddr;

    // Unknown protocol code (999)
    auto unknown_code = EncodeVarint(999);
    multiaddr.insert(multiaddr.end(), unknown_code.begin(), unknown_code.end());

    // Unknown protocol data length + data
    auto unknown_len = EncodeVarint(5);
    multiaddr.insert(multiaddr.end(), unknown_len.begin(), unknown_len.end());
    for (int i = 0; i < 5; ++i) {
        multiaddr.push_back(0xFF);
    }

    // IP4 protocol code (4)
    auto ip4_code = EncodeVarint(4);
    multiaddr.insert(multiaddr.end(), ip4_code.begin(), ip4_code.end());

    // IPv4 address
    uint8_t ip_bytes[4] = {192, 168, 1, 1};
    multiaddr.insert(multiaddr.end(), ip_bytes, ip_bytes + 4);

    // UDP protocol code (273)
    auto udp_code = EncodeVarint(273);
    multiaddr.insert(multiaddr.end(), udp_code.begin(), udp_code.end());

    // Port
    multiaddr.push_back(0x13);  // 5000 >> 8
    multiaddr.push_back(0x88);  // 5000 & 0xFF

    // Should skip unknown protocol and parse IP/UDP
    auto result = MultiaddrConverter::parse_udp(multiaddr);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->ip, "192.168.1.1");
    EXPECT_EQ(result->port, 5000);
}
