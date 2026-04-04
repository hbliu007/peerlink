#include <gtest/gtest.h>
#include <vector>
#include <cstdint>

// Forward declare functions to test
namespace p2p {
namespace varint {

// Encode unsigned integer to varint (LEB128)
std::vector<uint8_t> Encode(uint64_t value);

// Decode varint from buffer
// Returns: decoded value and number of bytes consumed
// Returns {0, 0} on error
std::pair<uint64_t, size_t> Decode(const uint8_t* data, size_t len);

}  // namespace varint
}  // namespace p2p

namespace {

using namespace p2p::varint;

// Test single byte encoding (0-127)
TEST(VarintTest, EncodeSingleByte) {
    EXPECT_EQ(Encode(0), std::vector<uint8_t>({0x00}));
    EXPECT_EQ(Encode(1), std::vector<uint8_t>({0x01}));
    EXPECT_EQ(Encode(127), std::vector<uint8_t>({0x7F}));
}

// Test two byte encoding (128-16383)
TEST(VarintTest, EncodeTwoBytes) {
    EXPECT_EQ(Encode(128), std::vector<uint8_t>({0x80, 0x01}));
    EXPECT_EQ(Encode(300), std::vector<uint8_t>({0xAC, 0x02}));
    EXPECT_EQ(Encode(16383), std::vector<uint8_t>({0xFF, 0x7F}));
}

// Test three byte encoding
TEST(VarintTest, EncodeThreeBytes) {
    EXPECT_EQ(Encode(16384), std::vector<uint8_t>({0x80, 0x80, 0x01}));
}

// Test protocol codes used in multiaddr
TEST(VarintTest, EncodeProtocolCodes) {
    EXPECT_EQ(Encode(4), std::vector<uint8_t>({0x04}));      // ip4
    EXPECT_EQ(Encode(6), std::vector<uint8_t>({0x06}));      // tcp
    EXPECT_EQ(Encode(41), std::vector<uint8_t>({0x29}));     // ip6
    EXPECT_EQ(Encode(273), std::vector<uint8_t>({0x91, 0x02}));  // udp
    EXPECT_EQ(Encode(421), std::vector<uint8_t>({0xA5, 0x03}));  // p2p
}

// Test decode single byte
TEST(VarintTest, DecodeSingleByte) {
    uint8_t data[] = {0x00};
    auto [value, consumed] = Decode(data, sizeof(data));
    EXPECT_EQ(value, 0);
    EXPECT_EQ(consumed, 1);

    uint8_t data2[] = {0x7F};
    auto [value2, consumed2] = Decode(data2, sizeof(data2));
    EXPECT_EQ(value2, 127);
    EXPECT_EQ(consumed2, 1);
}

// Test decode two bytes
TEST(VarintTest, DecodeTwoBytes) {
    uint8_t data[] = {0x80, 0x01};
    auto [value, consumed] = Decode(data, sizeof(data));
    EXPECT_EQ(value, 128);
    EXPECT_EQ(consumed, 2);

    uint8_t data2[] = {0xAC, 0x02};
    auto [value2, consumed2] = Decode(data2, sizeof(data2));
    EXPECT_EQ(value2, 300);
    EXPECT_EQ(consumed2, 2);
}

// Test decode error cases
TEST(VarintTest, DecodeErrors) {
    // Empty buffer
    auto [value1, consumed1] = Decode(nullptr, 0);
    EXPECT_EQ(value1, 0);
    EXPECT_EQ(consumed1, 0);

    // Incomplete varint
    uint8_t data[] = {0x80};  // Continuation bit set but no next byte
    auto [value2, consumed2] = Decode(data, sizeof(data));
    EXPECT_EQ(value2, 0);
    EXPECT_EQ(consumed2, 0);
}

// Test round-trip encoding/decoding
TEST(VarintTest, RoundTrip) {
    std::vector<uint64_t> test_values = {
        0, 1, 127, 128, 255, 256, 16383, 16384, 65535, 65536,
        1048575, 1048576, 268435455, 268435456
    };

    for (uint64_t original : test_values) {
        auto encoded = Encode(original);
        auto [decoded, consumed] = Decode(encoded.data(), encoded.size());
        EXPECT_EQ(decoded, original) << "Failed for value: " << original;
        EXPECT_EQ(consumed, encoded.size());
    }
}

}  // namespace
