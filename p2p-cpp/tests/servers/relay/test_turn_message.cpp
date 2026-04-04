#include <gtest/gtest.h>
#include "p2p/servers/relay/turn_message.hpp"
#include <cstring>

using namespace p2p::relay;

class TurnMessageTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(TurnMessageTest, ParseAllocateRequest) {
    uint8_t buffer[20];
    std::memset(buffer, 0, sizeof(buffer));

    // Message type: ALLOCATE (0x0003)
    buffer[0] = 0x00;
    buffer[1] = 0x03;

    // Message length: 0
    buffer[2] = 0x00;
    buffer[3] = 0x00;

    // Magic cookie: 0x2112A442
    buffer[4] = 0x21;
    buffer[5] = 0x12;
    buffer[6] = 0xA4;
    buffer[7] = 0x42;

    // Transaction ID: random 12 bytes
    for (int i = 8; i < 20; ++i) {
        buffer[i] = static_cast<uint8_t>(i);
    }

    auto msg = StunMessage::Parse(buffer, sizeof(buffer));
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->message_type, MessageType::ALLOCATE_REQUEST);
    EXPECT_EQ(msg->magic_cookie, MAGIC_COOKIE);
}

TEST_F(TurnMessageTest, ParseRefreshRequest) {
    uint8_t buffer[20];
    std::memset(buffer, 0, sizeof(buffer));

    // Message type: REFRESH (0x0004)
    buffer[0] = 0x00;
    buffer[1] = 0x04;

    // Magic cookie
    buffer[4] = 0x21;
    buffer[5] = 0x12;
    buffer[6] = 0xA4;
    buffer[7] = 0x42;

    auto msg = StunMessage::Parse(buffer, sizeof(buffer));
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->message_type, MessageType::REFRESH_REQUEST);
}

TEST_F(TurnMessageTest, ParseInvalidMagicCookie) {
    uint8_t buffer[20];
    std::memset(buffer, 0, sizeof(buffer));

    buffer[0] = 0x00;
    buffer[1] = 0x03;

    // Invalid magic cookie
    buffer[4] = 0xFF;
    buffer[5] = 0xFF;
    buffer[6] = 0xFF;
    buffer[7] = 0xFF;

    auto msg = StunMessage::Parse(buffer, sizeof(buffer));
    EXPECT_EQ(msg, nullptr);
}

TEST_F(TurnMessageTest, ParseMessageTooShort) {
    uint8_t buffer[10];
    std::memset(buffer, 0, sizeof(buffer));

    auto msg = StunMessage::Parse(buffer, sizeof(buffer));
    EXPECT_EQ(msg, nullptr);
}

TEST_F(TurnMessageTest, SerializeDeserialize) {
    StunMessage msg1;
    msg1.message_type = MessageType::ALLOCATE_REQUEST;
    msg1.magic_cookie = MAGIC_COOKIE;

    for (int i = 0; i < 12; ++i) {
        msg1.transaction_id[i] = static_cast<uint8_t>(i * 7);
    }

    std::vector<uint8_t> serialized = msg1.Serialize();
    EXPECT_GE(serialized.size(), 20u);

    auto msg2 = StunMessage::Parse(serialized.data(), serialized.size());
    ASSERT_NE(msg2, nullptr);
    EXPECT_EQ(msg2->message_type, msg1.message_type);
    EXPECT_EQ(msg2->transaction_id, msg1.transaction_id);
}

TEST_F(TurnMessageTest, AddAndGetAttribute) {
    StunMessage msg;
    msg.message_type = MessageType::ALLOCATE_REQUEST;

    std::vector<uint8_t> lifetime_data = {0x00, 0x00, 0x02, 0x58};  // 600 seconds
    msg.AddAttribute(AttributeType::LIFETIME, lifetime_data);

    const auto* attr = msg.GetAttribute(AttributeType::LIFETIME);
    ASSERT_NE(attr, nullptr);
    EXPECT_EQ(attr->type, AttributeType::LIFETIME);
    EXPECT_EQ(attr->value, lifetime_data);
}

TEST_F(TurnMessageTest, CreateLifetimeAttribute) {
    auto lifetime_data = CreateLifetimeAttr(600);
    EXPECT_EQ(lifetime_data.size(), 4u);

    uint32_t lifetime = ParseLifetimeAttr(lifetime_data);
    EXPECT_EQ(lifetime, 600u);
}

TEST_F(TurnMessageTest, CreateErrorCodeAttribute) {
    auto error_data = CreateErrorCodeAttr(ErrorCode::ALLOCATION_QUOTA_REACHED, "Quota exceeded");
    EXPECT_GT(error_data.size(), 4u);
}

TEST_F(TurnMessageTest, AddressStructure) {
    Address addr1("192.168.1.100", 3478);
    Address addr2("192.168.1.100", 3478);
    Address addr3("192.168.1.101", 3478);

    EXPECT_EQ(addr1, addr2);
    EXPECT_NE(addr1, addr3);
    EXPECT_EQ(addr1.ToString(), "192.168.1.100:3478");
}

TEST_F(TurnMessageTest, ParseCreatePermissionRequest) {
    uint8_t buffer[20];
    std::memset(buffer, 0, sizeof(buffer));

    // Message type: CREATE_PERMISSION (0x0008)
    buffer[0] = 0x00;
    buffer[1] = 0x08;

    buffer[4] = 0x21;
    buffer[5] = 0x12;
    buffer[6] = 0xA4;
    buffer[7] = 0x42;

    auto msg = StunMessage::Parse(buffer, sizeof(buffer));
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->message_type, MessageType::CREATE_PERMISSION_REQUEST);
}

TEST_F(TurnMessageTest, ParseSendIndication) {
    uint8_t buffer[20];
    std::memset(buffer, 0, sizeof(buffer));

    // Message type: SEND (0x0006)
    buffer[0] = 0x00;
    buffer[1] = 0x06;

    buffer[4] = 0x21;
    buffer[5] = 0x12;
    buffer[6] = 0xA4;
    buffer[7] = 0x42;

    auto msg = StunMessage::Parse(buffer, sizeof(buffer));
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->message_type, MessageType::SEND_INDICATION);
}

TEST_F(TurnMessageTest, ParseDataIndication) {
    uint8_t buffer[20];
    std::memset(buffer, 0, sizeof(buffer));

    // Message type: DATA (0x0007)
    buffer[0] = 0x00;
    buffer[1] = 0x07;

    buffer[4] = 0x21;
    buffer[5] = 0x12;
    buffer[6] = 0xA4;
    buffer[7] = 0x42;

    auto msg = StunMessage::Parse(buffer, sizeof(buffer));
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->message_type, MessageType::DATA_INDICATION);
}

TEST_F(TurnMessageTest, TransactionIdUniqueness) {
    StunMessage msg1, msg2;

    for (int i = 0; i < 12; ++i) {
        msg1.transaction_id[i] = static_cast<uint8_t>(i);
        msg2.transaction_id[i] = static_cast<uint8_t>(i + 100);
    }

    EXPECT_NE(msg1.transaction_id, msg2.transaction_id);
}

TEST_F(TurnMessageTest, MessageLengthValidation) {
    uint8_t buffer[20];
    std::memset(buffer, 0, sizeof(buffer));

    buffer[0] = 0x00;
    buffer[1] = 0x03;

    // Invalid length (larger than actual buffer)
    buffer[2] = 0xFF;
    buffer[3] = 0xFF;

    buffer[4] = 0x21;
    buffer[5] = 0x12;
    buffer[6] = 0xA4;
    buffer[7] = 0x42;

    auto msg = StunMessage::Parse(buffer, sizeof(buffer));
    EXPECT_EQ(msg, nullptr);
}

TEST_F(TurnMessageTest, MultipleAttributes) {
    StunMessage msg;
    msg.message_type = MessageType::ALLOCATE_REQUEST;

    msg.AddAttribute(AttributeType::LIFETIME, CreateLifetimeAttr(600));
    msg.AddAttribute(AttributeType::REQUESTED_TRANSPORT, {0x11, 0x00, 0x00, 0x00});

    EXPECT_NE(msg.GetAttribute(AttributeType::LIFETIME), nullptr);
    EXPECT_NE(msg.GetAttribute(AttributeType::REQUESTED_TRANSPORT), nullptr);
    EXPECT_EQ(msg.GetAttribute(AttributeType::USERNAME), nullptr);
}

TEST_F(TurnMessageTest, XorAddressEncodeDecode) {
    Address original("192.168.1.100", 54321);
    std::array<uint8_t, 12> transaction_id = {0};

    auto encoded = CreateXorAddressAttr(original, transaction_id);
    EXPECT_EQ(encoded.size(), 8u);  // 2 bytes family + 2 bytes port + 4 bytes IP

    auto decoded = ParseXorAddressAttr(encoded, transaction_id);
    EXPECT_EQ(decoded.ip, original.ip);
    EXPECT_EQ(decoded.port, original.port);
}

TEST_F(TurnMessageTest, XorAddressDifferentIPs) {
    std::array<uint8_t, 12> transaction_id = {0};

    Address addr1("10.0.0.1", 3478);
    Address addr2("172.16.0.1", 5000);
    Address addr3("192.168.1.1", 8080);

    auto encoded1 = CreateXorAddressAttr(addr1, transaction_id);
    auto encoded2 = CreateXorAddressAttr(addr2, transaction_id);
    auto encoded3 = CreateXorAddressAttr(addr3, transaction_id);

    auto decoded1 = ParseXorAddressAttr(encoded1, transaction_id);
    auto decoded2 = ParseXorAddressAttr(encoded2, transaction_id);
    auto decoded3 = ParseXorAddressAttr(encoded3, transaction_id);

    EXPECT_EQ(decoded1.ip, addr1.ip);
    EXPECT_EQ(decoded1.port, addr1.port);
    EXPECT_EQ(decoded2.ip, addr2.ip);
    EXPECT_EQ(decoded2.port, addr2.port);
    EXPECT_EQ(decoded3.ip, addr3.ip);
    EXPECT_EQ(decoded3.port, addr3.port);
}

TEST_F(TurnMessageTest, ParseXorAddressInsufficientData) {
    std::array<uint8_t, 12> transaction_id = {0};
    std::vector<uint8_t> short_data = {0x00, 0x01, 0x12, 0x34};  // Only 4 bytes

    auto addr = ParseXorAddressAttr(short_data, transaction_id);
    // Should return default-constructed Address
    EXPECT_TRUE(addr.ip.empty() || addr.port == 0);
}

TEST_F(TurnMessageTest, CreateErrorResponse) {
    std::array<uint8_t, 12> transaction_id;
    for (int i = 0; i < 12; ++i) {
        transaction_id[i] = static_cast<uint8_t>(i * 3);
    }

    auto error_msg = CreateErrorResponse(
        transaction_id,
        ErrorCode::ALLOCATION_QUOTA_REACHED,
        "Quota exceeded"
    );

    ASSERT_NE(error_msg, nullptr);
    EXPECT_EQ(error_msg->message_type, MessageType::BINDING_ERROR_RESPONSE);
    EXPECT_EQ(error_msg->magic_cookie, MAGIC_COOKIE);
    EXPECT_EQ(error_msg->transaction_id, transaction_id);

    const auto* error_attr = error_msg->GetAttribute(AttributeType::ERROR_CODE);
    ASSERT_NE(error_attr, nullptr);
    EXPECT_GT(error_attr->value.size(), 4u);
}

TEST_F(TurnMessageTest, CreateErrorResponseDifferentCodes) {
    std::array<uint8_t, 12> transaction_id = {0};

    auto err400 = CreateErrorResponse(transaction_id, ErrorCode::BAD_REQUEST, "Bad request");
    auto err401 = CreateErrorResponse(transaction_id, ErrorCode::UNAUTHORIZED, "Unauthorized");
    auto err438 = CreateErrorResponse(transaction_id, ErrorCode::STALE_NONCE, "Stale nonce");

    ASSERT_NE(err400, nullptr);
    ASSERT_NE(err401, nullptr);
    ASSERT_NE(err438, nullptr);

    EXPECT_NE(err400->GetAttribute(AttributeType::ERROR_CODE), nullptr);
    EXPECT_NE(err401->GetAttribute(AttributeType::ERROR_CODE), nullptr);
    EXPECT_NE(err438->GetAttribute(AttributeType::ERROR_CODE), nullptr);
}

TEST_F(TurnMessageTest, ParseLifetimeInsufficientData) {
    std::vector<uint8_t> short_data = {0x00, 0x01};  // Only 2 bytes
    uint32_t lifetime = ParseLifetimeAttr(short_data);
    EXPECT_EQ(lifetime, 0u);
}

TEST_F(TurnMessageTest, ParseLifetimeZero) {
    auto lifetime_data = CreateLifetimeAttr(0);
    uint32_t lifetime = ParseLifetimeAttr(lifetime_data);
    EXPECT_EQ(lifetime, 0u);
}

TEST_F(TurnMessageTest, ParseLifetimeMaxValue) {
    auto lifetime_data = CreateLifetimeAttr(0xFFFFFFFF);
    uint32_t lifetime = ParseLifetimeAttr(lifetime_data);
    EXPECT_EQ(lifetime, 0xFFFFFFFF);
}

TEST_F(TurnMessageTest, SerializeWithAttributes) {
    StunMessage msg;
    msg.message_type = MessageType::ALLOCATE_REQUEST;
    msg.magic_cookie = MAGIC_COOKIE;
    for (int i = 0; i < 12; ++i) {
        msg.transaction_id[i] = static_cast<uint8_t>(i);
    }

    msg.AddAttribute(AttributeType::LIFETIME, CreateLifetimeAttr(600));
    msg.AddAttribute(AttributeType::REQUESTED_TRANSPORT, {0x11, 0x00, 0x00, 0x00});

    auto serialized = msg.Serialize();
    EXPECT_GT(serialized.size(), 20u);  // Header + attributes

    auto parsed = StunMessage::Parse(serialized.data(), serialized.size());
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->message_type, msg.message_type);
    EXPECT_EQ(parsed->attributes.size(), 2u);
}

TEST_F(TurnMessageTest, ParseAttributesWithPadding) {
    std::vector<uint8_t> buffer;

    // Header
    buffer.push_back(0x00); buffer.push_back(0x03);  // ALLOCATE_REQUEST
    buffer.push_back(0x00); buffer.push_back(0x08);  // Length: 8 bytes (4 header + 3 value + 1 padding)
    buffer.push_back(0x21); buffer.push_back(0x12);  // Magic cookie
    buffer.push_back(0xA4); buffer.push_back(0x42);
    for (int i = 0; i < 12; ++i) {
        buffer.push_back(static_cast<uint8_t>(i));  // Transaction ID
    }

    // Attribute with 3-byte value (needs 1 byte padding)
    buffer.push_back(0x00); buffer.push_back(0x13);  // DATA attribute
    buffer.push_back(0x00); buffer.push_back(0x03);  // Length: 3 bytes
    buffer.push_back(0xAA); buffer.push_back(0xBB); buffer.push_back(0xCC);  // Value
    buffer.push_back(0x00);  // Padding

    auto msg = StunMessage::Parse(buffer.data(), buffer.size());
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->attributes.size(), 1u);
    EXPECT_EQ(msg->attributes[0].value.size(), 3u);
}

TEST_F(TurnMessageTest, ErrorCodeAttributeStructure) {
    auto error_data = CreateErrorCodeAttr(ErrorCode::ALLOCATION_QUOTA_REACHED, "Quota");

    ASSERT_GE(error_data.size(), 4u);
    // First 2 bytes should be 0
    EXPECT_EQ(error_data[0], 0);
    EXPECT_EQ(error_data[1], 0);
    // Class digit (4 for 4xx errors)
    EXPECT_EQ(error_data[2], 4);
    // Number (86 for 486)
    EXPECT_EQ(error_data[3], 86);
}

TEST_F(TurnMessageTest, GetNonExistentAttribute) {
    StunMessage msg;
    msg.message_type = MessageType::ALLOCATE_REQUEST;

    const auto* attr = msg.GetAttribute(AttributeType::USERNAME);
    EXPECT_EQ(attr, nullptr);
}

TEST_F(TurnMessageTest, ParseTruncatedAttribute) {
    std::vector<uint8_t> buffer;

    // Header
    buffer.push_back(0x00); buffer.push_back(0x03);
    buffer.push_back(0x00); buffer.push_back(0x08);  // Claims 8 bytes of attributes
    buffer.push_back(0x21); buffer.push_back(0x12);
    buffer.push_back(0xA4); buffer.push_back(0x42);
    for (int i = 0; i < 12; ++i) {
        buffer.push_back(static_cast<uint8_t>(i));
    }

    // Attribute header but incomplete value
    buffer.push_back(0x00); buffer.push_back(0x0D);  // LIFETIME
    buffer.push_back(0x00); buffer.push_back(0x04);  // Length: 4 bytes
    buffer.push_back(0x00); buffer.push_back(0x01);  // Only 2 bytes of value

    auto msg = StunMessage::Parse(buffer.data(), buffer.size());
    // Parser should reject truncated attributes
    EXPECT_EQ(msg, nullptr);
}

TEST_F(TurnMessageTest, ParseExcessiveAttributeLength) {
    std::vector<uint8_t> buffer;

    // Header
    buffer.push_back(0x00); buffer.push_back(0x03);  // ALLOCATE_REQUEST
    buffer.push_back(0x00); buffer.push_back(0x08);  // Message length: 8 bytes
    buffer.push_back(0x21); buffer.push_back(0x12);  // Magic cookie
    buffer.push_back(0xA4); buffer.push_back(0x42);
    for (int i = 0; i < 12; ++i) {
        buffer.push_back(static_cast<uint8_t>(i));  // Transaction ID
    }

    // Attribute with excessive length (0xFFFF = 65535 bytes)
    buffer.push_back(0x00); buffer.push_back(0x0D);  // LIFETIME
    buffer.push_back(0xFF); buffer.push_back(0xFF);  // Length: 65535 bytes (exceeds remaining)
    buffer.push_back(0x00); buffer.push_back(0x00);
    buffer.push_back(0x00); buffer.push_back(0x00);

    auto msg = StunMessage::Parse(buffer.data(), buffer.size());
    // Parser should reject attributes with length exceeding remaining bytes
    EXPECT_EQ(msg, nullptr);
}

TEST_F(TurnMessageTest, ParseIntegerOverflowPadding) {
    std::vector<uint8_t> buffer;

    // Header
    buffer.push_back(0x00); buffer.push_back(0x03);  // ALLOCATE_REQUEST
    buffer.push_back(0x00); buffer.push_back(0x08);  // Message length: 8 bytes
    buffer.push_back(0x21); buffer.push_back(0x12);  // Magic cookie
    buffer.push_back(0xA4); buffer.push_back(0x42);
    for (int i = 0; i < 12; ++i) {
        buffer.push_back(static_cast<uint8_t>(i));  // Transaction ID
    }

    // Attribute with length that could cause integer overflow when padded
    buffer.push_back(0x00); buffer.push_back(0x0D);  // LIFETIME
    buffer.push_back(0xFF); buffer.push_back(0xFD);  // Length: 65533 (padding would overflow)
    buffer.push_back(0x00); buffer.push_back(0x00);
    buffer.push_back(0x00); buffer.push_back(0x00);

    auto msg = StunMessage::Parse(buffer.data(), buffer.size());
    // Parser should detect potential integer overflow
    EXPECT_EQ(msg, nullptr);
}
