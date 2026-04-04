#include "p2p/protocol/stun.hpp"
#include <gtest/gtest.h>
#include <arpa/inet.h>

using namespace p2p::protocol;

class StunProtocolTest : public ::testing::Test {
protected:
    // Helper: Create test transaction ID
    TransactionId CreateTestTransactionId(uint8_t fill_byte = 0xAB) {
        TransactionId tid;
        tid.fill(fill_byte);
        return tid;
    }

    // Helper: Create transaction ID with pattern
    TransactionId CreatePatternTransactionId() {
        TransactionId tid;
        for (size_t i = 0; i < 12; ++i) {
            tid[i] = static_cast<uint8_t>(i);
        }
        return tid;
    }

    // Helper: Verify STUN message header format
    bool VerifyHeader(const std::vector<uint8_t>& data,
                     StunMessageType expected_type,
                     const TransactionId& expected_tid) {
        if (data.size() < 20) return false;

        // Check message type
        uint16_t type = ntohs(*reinterpret_cast<const uint16_t*>(data.data()));
        if (type != static_cast<uint16_t>(expected_type)) return false;

        // Check magic cookie
        uint32_t cookie = ntohl(*reinterpret_cast<const uint32_t*>(data.data() + 4));
        if (cookie != STUN_MAGIC_COOKIE) return false;

        // Check transaction ID
        for (size_t i = 0; i < 12; ++i) {
            if (data[8 + i] != expected_tid[i]) return false;
        }

        return true;
    }
};

// --- StunMessage Construction Tests ---

TEST_F(StunProtocolTest, Constructor_SetsTypeAndTransactionId) {
    auto tid = CreateTestTransactionId();
    StunMessage msg(StunMessageType::BindingRequest, tid);

    EXPECT_EQ(msg.message_type(), StunMessageType::BindingRequest);
    EXPECT_EQ(msg.transaction_id(), tid);
    EXPECT_TRUE(msg.attributes().empty());
}

TEST_F(StunProtocolTest, AddAttribute_AddsToList) {
    auto tid = CreateTestTransactionId();
    StunMessage msg(StunMessageType::BindingRequest, tid);

    std::vector<uint8_t> value = {0x01, 0x02, 0x03};
    msg.add_attribute(StunAttribute(StunAttributeType::Software, value));

    EXPECT_EQ(msg.attributes().size(), 1);
    EXPECT_EQ(msg.attributes()[0].type, StunAttributeType::Software);
    EXPECT_EQ(msg.attributes()[0].value, value);
}

TEST_F(StunProtocolTest, AddMultipleAttributes_AllAdded) {
    auto tid = CreateTestTransactionId();
    StunMessage msg(StunMessageType::BindingRequest, tid);

    msg.add_attribute(StunAttribute(StunAttributeType::Software, {0x01}));
    msg.add_attribute(StunAttribute(StunAttributeType::XorMappedAddress, {0x02, 0x03}));
    msg.add_attribute(StunAttribute(StunAttributeType::ErrorCode, {0x04, 0x05, 0x06}));

    EXPECT_EQ(msg.attributes().size(), 3);
}

TEST_F(StunProtocolTest, GetAttribute_ExistingAttribute_ReturnsAttribute) {
    auto tid = CreateTestTransactionId();
    StunMessage msg(StunMessageType::BindingRequest, tid);

    std::vector<uint8_t> value = {0xDE, 0xAD, 0xBE, 0xEF};
    msg.add_attribute(StunAttribute(StunAttributeType::Software, value));

    auto result = msg.get_attribute(StunAttributeType::Software);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)->type, StunAttributeType::Software);
    EXPECT_EQ((*result)->value, value);
}

TEST_F(StunProtocolTest, GetAttribute_NonExistingAttribute_ReturnsNullopt) {
    auto tid = CreateTestTransactionId();
    StunMessage msg(StunMessageType::BindingRequest, tid);

    auto result = msg.get_attribute(StunAttributeType::Software);
    EXPECT_FALSE(result.has_value());
}

TEST_F(StunProtocolTest, GetAttribute_MultipleAttributes_ReturnsFirst) {
    auto tid = CreateTestTransactionId();
    StunMessage msg(StunMessageType::BindingRequest, tid);

    msg.add_attribute(StunAttribute(StunAttributeType::Software, {0x01}));
    msg.add_attribute(StunAttribute(StunAttributeType::Software, {0x02}));

    auto result = msg.get_attribute(StunAttributeType::Software);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)->value, std::vector<uint8_t>{0x01});
}

// --- Serialization Tests ---

TEST_F(StunProtocolTest, Serialize_EmptyMessage_CreatesValidHeader) {
    auto tid = CreateTestTransactionId();
    StunMessage msg(StunMessageType::BindingRequest, tid);

    auto data = msg.serialize();

    EXPECT_EQ(data.size(), 20);  // Header only
    EXPECT_TRUE(VerifyHeader(data, StunMessageType::BindingRequest, tid));

    // Check message length is 0
    uint16_t length = ntohs(*reinterpret_cast<const uint16_t*>(data.data() + 2));
    EXPECT_EQ(length, 0);
}

TEST_F(StunProtocolTest, Serialize_WithSingleAttribute_IncludesAttribute) {
    auto tid = CreateTestTransactionId();
    StunMessage msg(StunMessageType::BindingResponse, tid);

    std::vector<uint8_t> attr_value = {0x01, 0x02, 0x03, 0x04};
    msg.add_attribute(StunAttribute(StunAttributeType::Software, attr_value));

    auto data = msg.serialize();

    // Header (20) + Attribute header (4) + Value (4) = 28
    EXPECT_EQ(data.size(), 28);
    EXPECT_TRUE(VerifyHeader(data, StunMessageType::BindingResponse, tid));

    // Check message length
    uint16_t length = ntohs(*reinterpret_cast<const uint16_t*>(data.data() + 2));
    EXPECT_EQ(length, 8);  // Attribute header (4) + value (4)
}

TEST_F(StunProtocolTest, Serialize_AttributeWithPadding_AddsPadding) {
    auto tid = CreateTestTransactionId();
    StunMessage msg(StunMessageType::BindingRequest, tid);

    // 3-byte value requires 1 byte padding to align to 4 bytes
    std::vector<uint8_t> attr_value = {0x01, 0x02, 0x03};
    msg.add_attribute(StunAttribute(StunAttributeType::Software, attr_value));

    auto data = msg.serialize();

    // Header (20) + Attribute header (4) + Value (3) + Padding (1) = 28
    EXPECT_EQ(data.size(), 28);

    // Verify padding byte is 0
    EXPECT_EQ(data[27], 0);
}

TEST_F(StunProtocolTest, Serialize_MultipleAttributes_AllIncluded) {
    auto tid = CreateTestTransactionId();
    StunMessage msg(StunMessageType::BindingRequest, tid);

    msg.add_attribute(StunAttribute(StunAttributeType::Software, {0x01, 0x02}));
    msg.add_attribute(StunAttribute(StunAttributeType::ErrorCode, {0x03, 0x04, 0x05}));

    auto data = msg.serialize();

    // Header (20) + Attr1 (4+2+2 padding) + Attr2 (4+3+1 padding) = 36
    EXPECT_EQ(data.size(), 36);
}

// --- Deserialization Tests ---

TEST_F(StunProtocolTest, Parse_ValidEmptyMessage_Succeeds) {
    auto tid = CreateTestTransactionId();
    StunMessage original(StunMessageType::BindingRequest, tid);
    auto data = original.serialize();

    auto parsed = StunMessage::parse(data.data(), data.size());

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->message_type(), StunMessageType::BindingRequest);
    EXPECT_EQ(parsed->transaction_id(), tid);
    EXPECT_TRUE(parsed->attributes().empty());
}

TEST_F(StunProtocolTest, Parse_WithSingleAttribute_ParsesCorrectly) {
    auto tid = CreateTestTransactionId();
    StunMessage original(StunMessageType::BindingResponse, tid);
    std::vector<uint8_t> attr_value = {0xAA, 0xBB, 0xCC, 0xDD};
    original.add_attribute(StunAttribute(StunAttributeType::Software, attr_value));

    auto data = original.serialize();
    auto parsed = StunMessage::parse(data.data(), data.size());

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->attributes().size(), 1);
    EXPECT_EQ(parsed->attributes()[0].type, StunAttributeType::Software);
    EXPECT_EQ(parsed->attributes()[0].value, attr_value);
}

TEST_F(StunProtocolTest, Parse_WithPaddedAttribute_IgnoresPadding) {
    auto tid = CreateTestTransactionId();
    StunMessage original(StunMessageType::BindingRequest, tid);
    std::vector<uint8_t> attr_value = {0x01, 0x02, 0x03};  // 3 bytes, needs padding
    original.add_attribute(StunAttribute(StunAttributeType::Software, attr_value));

    auto data = original.serialize();
    auto parsed = StunMessage::parse(data.data(), data.size());

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->attributes().size(), 1);
    EXPECT_EQ(parsed->attributes()[0].value, attr_value);  // Padding not included
}

TEST_F(StunProtocolTest, Parse_TooShort_ReturnsNullopt) {
    std::vector<uint8_t> short_data(19, 0);  // Less than 20 bytes
    auto result = StunMessage::parse(short_data.data(), short_data.size());
    EXPECT_FALSE(result.has_value());
}

TEST_F(StunProtocolTest, Parse_InvalidMagicCookie_ReturnsNullopt) {
    std::vector<uint8_t> data(20, 0);
    // Set wrong magic cookie
    uint32_t wrong_cookie = htonl(0x12345678);
    std::memcpy(data.data() + 4, &wrong_cookie, 4);

    auto result = StunMessage::parse(data.data(), data.size());
    EXPECT_FALSE(result.has_value());
}

TEST_F(StunProtocolTest, Parse_TruncatedAttribute_ReturnsNullopt) {
    auto tid = CreateTestTransactionId();
    StunMessage msg(StunMessageType::BindingRequest, tid);
    msg.add_attribute(StunAttribute(StunAttributeType::Software, {0x01, 0x02, 0x03, 0x04}));

    auto data = msg.serialize();
    // Truncate last 2 bytes
    data.resize(data.size() - 2);

    auto result = StunMessage::parse(data.data(), data.size());
    EXPECT_FALSE(result.has_value());
}

TEST_F(StunProtocolTest, Parse_AttributeLengthExceedsRemaining_ReturnsNullopt) {
    std::vector<uint8_t> data(20, 0);

    // Valid header
    uint16_t type = htons(static_cast<uint16_t>(StunMessageType::BindingRequest));
    std::memcpy(data.data(), &type, 2);

    uint16_t length = htons(8);  // Claim 8 bytes of attributes
    std::memcpy(data.data() + 2, &length, 2);

    uint32_t cookie = htonl(STUN_MAGIC_COOKIE);
    std::memcpy(data.data() + 4, &cookie, 4);

    auto tid = CreateTestTransactionId();
    std::memcpy(data.data() + 8, tid.data(), 12);

    // Add attribute header claiming 100 bytes (but we don't have that much data)
    uint16_t attr_type = htons(static_cast<uint16_t>(StunAttributeType::Software));
    uint16_t attr_length = htons(100);
    data.insert(data.end(), reinterpret_cast<uint8_t*>(&attr_type),
                reinterpret_cast<uint8_t*>(&attr_type) + 2);
    data.insert(data.end(), reinterpret_cast<uint8_t*>(&attr_length),
                reinterpret_cast<uint8_t*>(&attr_length) + 2);

    auto result = StunMessage::parse(data.data(), data.size());
    EXPECT_FALSE(result.has_value());
}

// --- Roundtrip Tests ---

TEST_F(StunProtocolTest, SerializeDeserialize_EmptyMessage_Roundtrip) {
    auto tid = CreatePatternTransactionId();
    StunMessage original(StunMessageType::BindingRequest, tid);

    auto data = original.serialize();
    auto parsed = StunMessage::parse(data.data(), data.size());

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->message_type(), original.message_type());
    EXPECT_EQ(parsed->transaction_id(), original.transaction_id());
    EXPECT_EQ(parsed->attributes().size(), original.attributes().size());
}

TEST_F(StunProtocolTest, SerializeDeserialize_WithAttributes_Roundtrip) {
    auto tid = CreatePatternTransactionId();
    StunMessage original(StunMessageType::BindingResponse, tid);

    original.add_attribute(StunAttribute(StunAttributeType::Software, {0x01, 0x02, 0x03}));
    original.add_attribute(StunAttribute(StunAttributeType::ErrorCode, {0x04, 0x05}));

    auto data = original.serialize();
    auto parsed = StunMessage::parse(data.data(), data.size());

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->message_type(), original.message_type());
    EXPECT_EQ(parsed->transaction_id(), original.transaction_id());
    ASSERT_EQ(parsed->attributes().size(), 2);
    EXPECT_EQ(parsed->attributes()[0].type, StunAttributeType::Software);
    EXPECT_EQ(parsed->attributes()[0].value, std::vector<uint8_t>({0x01, 0x02, 0x03}));
    EXPECT_EQ(parsed->attributes()[1].type, StunAttributeType::ErrorCode);
    EXPECT_EQ(parsed->attributes()[1].value, std::vector<uint8_t>({0x04, 0x05}));
}

// --- XOR Mapped Address Tests ---

TEST_F(StunProtocolTest, CreateXorMappedAddress_IPv4_CreatesCorrectly) {
    auto tid = CreateTestTransactionId();
    std::string ip = "192.168.1.100";
    uint16_t port = 12345;

    auto data = create_xor_mapped_address(ip, port, tid);

    EXPECT_EQ(data.size(), 8);  // 1 + 1 + 2 + 4 for IPv4
    EXPECT_EQ(data[0], 0);  // Reserved
    EXPECT_EQ(data[1], static_cast<uint8_t>(AddressFamily::IPv4));
}

TEST_F(StunProtocolTest, CreateXorMappedAddress_IPv6_CreatesCorrectly) {
    auto tid = CreateTestTransactionId();
    std::string ip = "::1";
    uint16_t port = 8080;

    auto data = create_xor_mapped_address(ip, port, tid);

    EXPECT_EQ(data.size(), 20);  // 1 + 1 + 2 + 16 for IPv6
    EXPECT_EQ(data[0], 0);  // Reserved
    EXPECT_EQ(data[1], static_cast<uint8_t>(AddressFamily::IPv6));
}

TEST_F(StunProtocolTest, ParseXorMappedAddress_IPv4_ParsesCorrectly) {
    auto tid = CreateTestTransactionId();
    std::string ip = "203.0.113.1";
    uint16_t port = 54321;

    auto data = create_xor_mapped_address(ip, port, tid);
    auto result = parse_xor_mapped_address(data, tid);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first, ip);
    EXPECT_EQ(result->second, port);
}

TEST_F(StunProtocolTest, ParseXorMappedAddress_IPv6_ParsesCorrectly) {
    auto tid = CreateTestTransactionId();
    std::string ip = "fe80::1";
    uint16_t port = 9999;

    auto data = create_xor_mapped_address(ip, port, tid);
    auto result = parse_xor_mapped_address(data, tid);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first, ip);
    EXPECT_EQ(result->second, port);
}

TEST_F(StunProtocolTest, ParseXorMappedAddress_TooShort_ReturnsNullopt) {
    auto tid = CreateTestTransactionId();
    std::vector<uint8_t> short_data = {0x00, 0x01, 0x12, 0x34};  // Only 4 bytes

    auto result = parse_xor_mapped_address(short_data, tid);
    EXPECT_FALSE(result.has_value());
}

TEST_F(StunProtocolTest, XorMappedAddress_IPv4_Roundtrip) {
    auto tid = CreatePatternTransactionId();
    std::string ip = "10.0.0.1";
    uint16_t port = 3478;

    auto data = create_xor_mapped_address(ip, port, tid);
    auto result = parse_xor_mapped_address(data, tid);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first, ip);
    EXPECT_EQ(result->second, port);
}

TEST_F(StunProtocolTest, XorMappedAddress_IPv6_Roundtrip) {
    auto tid = CreatePatternTransactionId();
    std::string ip = "2001:db8::1";
    uint16_t port = 65535;

    auto data = create_xor_mapped_address(ip, port, tid);
    auto result = parse_xor_mapped_address(data, tid);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first, ip);
    EXPECT_EQ(result->second, port);
}

TEST_F(StunProtocolTest, XorMappedAddress_BoundaryPorts_Work) {
    auto tid = CreateTestTransactionId();

    // Port 0
    auto data0 = create_xor_mapped_address("127.0.0.1", 0, tid);
    auto result0 = parse_xor_mapped_address(data0, tid);
    ASSERT_TRUE(result0.has_value());
    EXPECT_EQ(result0->second, 0);

    // Port 65535
    auto data65535 = create_xor_mapped_address("127.0.0.1", 65535, tid);
    auto result65535 = parse_xor_mapped_address(data65535, tid);
    ASSERT_TRUE(result65535.has_value());
    EXPECT_EQ(result65535->second, 65535);
}

// --- Error Response Tests ---

TEST_F(StunProtocolTest, CreateErrorResponse_CreatesCorrectMessage) {
    auto tid = CreateTestTransactionId();
    auto msg = create_error_response(tid, StunErrorCode::BadRequest, "Bad Request");

    EXPECT_EQ(msg.message_type(), StunMessageType::BindingErrorResponse);
    EXPECT_EQ(msg.transaction_id(), tid);
    EXPECT_EQ(msg.attributes().size(), 1);

    auto attr = msg.get_attribute(StunAttributeType::ErrorCode);
    ASSERT_TRUE(attr.has_value());
}

TEST_F(StunProtocolTest, CreateErrorResponse_ErrorCodeFormat_Correct) {
    auto tid = CreateTestTransactionId();
    auto msg = create_error_response(tid, StunErrorCode::Unauthorized, "Unauthorized");

    auto attr = msg.get_attribute(StunAttributeType::ErrorCode);
    ASSERT_TRUE(attr.has_value());

    const auto& value = (*attr)->value;
    ASSERT_GE(value.size(), 4);

    EXPECT_EQ(value[0], 0);  // Reserved
    EXPECT_EQ(value[1], 0);  // Reserved
    EXPECT_EQ(value[2], 4);  // Class (401 / 100 = 4)
    EXPECT_EQ(value[3], 1);  // Number (401 % 100 = 1)
}

TEST_F(StunProtocolTest, CreateErrorResponse_IncludesReasonPhrase) {
    auto tid = CreateTestTransactionId();
    std::string reason = "Server Error";
    auto msg = create_error_response(tid, StunErrorCode::ServerError, reason);

    auto attr = msg.get_attribute(StunAttributeType::ErrorCode);
    ASSERT_TRUE(attr.has_value());

    const auto& value = (*attr)->value;
    ASSERT_GE(value.size(), 4 + reason.size());

    // Check reason phrase
    std::string parsed_reason(value.begin() + 4, value.end());
    EXPECT_EQ(parsed_reason, reason);
}

TEST_F(StunProtocolTest, CreateErrorResponse_EmptyReason_Works) {
    auto tid = CreateTestTransactionId();
    auto msg = create_error_response(tid, StunErrorCode::TryAlternate, "");

    auto attr = msg.get_attribute(StunAttributeType::ErrorCode);
    ASSERT_TRUE(attr.has_value());

    const auto& value = (*attr)->value;
    EXPECT_EQ(value.size(), 4);  // Only error code, no reason phrase
}

// --- Edge Cases and Security Tests ---

TEST_F(StunProtocolTest, LargeAttribute_HandledCorrectly) {
    auto tid = CreateTestTransactionId();
    StunMessage msg(StunMessageType::BindingRequest, tid);

    // Create large attribute (1000 bytes)
    std::vector<uint8_t> large_value(1000, 0xFF);
    msg.add_attribute(StunAttribute(StunAttributeType::Software, large_value));

    auto data = msg.serialize();
    auto parsed = StunMessage::parse(data.data(), data.size());

    ASSERT_TRUE(parsed.has_value());
    ASSERT_EQ(parsed->attributes().size(), 1);
    EXPECT_EQ(parsed->attributes()[0].value, large_value);
}

TEST_F(StunProtocolTest, AllZeroTransactionId_Works) {
    TransactionId tid;
    tid.fill(0);
    StunMessage msg(StunMessageType::BindingRequest, tid);

    auto data = msg.serialize();
    auto parsed = StunMessage::parse(data.data(), data.size());

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->transaction_id(), tid);
}

TEST_F(StunProtocolTest, AllOnesTransactionId_Works) {
    TransactionId tid;
    tid.fill(0xFF);
    StunMessage msg(StunMessageType::BindingRequest, tid);

    auto data = msg.serialize();
    auto parsed = StunMessage::parse(data.data(), data.size());

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->transaction_id(), tid);
}

TEST_F(StunProtocolTest, DifferentMessageTypes_SerializeCorrectly) {
    auto tid = CreateTestTransactionId();

    StunMessage req(StunMessageType::BindingRequest, tid);
    auto req_data = req.serialize();
    uint16_t req_type = ntohs(*reinterpret_cast<const uint16_t*>(req_data.data()));
    EXPECT_EQ(req_type, 0x0001);

    StunMessage resp(StunMessageType::BindingResponse, tid);
    auto resp_data = resp.serialize();
    uint16_t resp_type = ntohs(*reinterpret_cast<const uint16_t*>(resp_data.data()));
    EXPECT_EQ(resp_type, 0x0101);

    StunMessage err(StunMessageType::BindingErrorResponse, tid);
    auto err_data = err.serialize();
    uint16_t err_type = ntohs(*reinterpret_cast<const uint16_t*>(err_data.data()));
    EXPECT_EQ(err_type, 0x0111);
}
