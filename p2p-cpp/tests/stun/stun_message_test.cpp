#include "p2p/protocol/stun.hpp"
#include <gtest/gtest.h>

using namespace p2p::protocol;

class StunMessageTest : public ::testing::Test {
protected:
    TransactionId test_transaction_id;

    void SetUp() override {
        // Create a test transaction ID
        for (size_t i = 0; i < 12; ++i) {
            test_transaction_id[i] = static_cast<uint8_t>(i);
        }
    }
};

TEST_F(StunMessageTest, SerializeBindingRequest) {
    StunMessage message(StunMessageType::BindingRequest, test_transaction_id);

    auto data = message.serialize();

    // Check minimum size (20 bytes header)
    ASSERT_GE(data.size(), 20);

    // Check message type
    uint16_t message_type = ntohs(*reinterpret_cast<const uint16_t*>(data.data()));
    EXPECT_EQ(message_type, 0x0001);

    // Check magic cookie
    uint32_t magic_cookie = ntohl(*reinterpret_cast<const uint32_t*>(data.data() + 4));
    EXPECT_EQ(magic_cookie, STUN_MAGIC_COOKIE);

    // Check transaction ID
    for (size_t i = 0; i < 12; ++i) {
        EXPECT_EQ(data[8 + i], test_transaction_id[i]);
    }
}

TEST_F(StunMessageTest, ParseBindingRequest) {
    StunMessage original(StunMessageType::BindingRequest, test_transaction_id);
    auto data = original.serialize();

    auto parsed = StunMessage::parse(data.data(), data.size());

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->message_type(), StunMessageType::BindingRequest);
    EXPECT_EQ(parsed->transaction_id(), test_transaction_id);
}

TEST_F(StunMessageTest, SerializeWithAttribute) {
    StunMessage message(StunMessageType::BindingResponse, test_transaction_id);

    // Add a simple attribute
    std::vector<uint8_t> attr_value = {0x01, 0x02, 0x03, 0x04};
    message.add_attribute(StunAttribute(StunAttributeType::MappedAddress, attr_value));

    auto data = message.serialize();

    // Parse back
    auto parsed = StunMessage::parse(data.data(), data.size());

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->attributes().size(), 1);

    auto attr = parsed->get_attribute(StunAttributeType::MappedAddress);
    ASSERT_TRUE(attr.has_value());
    EXPECT_EQ((*attr)->value, attr_value);
}

TEST_F(StunMessageTest, XorMappedAddressIPv4) {
    std::string test_ip = "192.168.1.100";
    uint16_t test_port = 12345;

    auto xor_data = create_xor_mapped_address(test_ip, test_port, test_transaction_id);

    ASSERT_FALSE(xor_data.empty());

    // Parse back
    auto result = parse_xor_mapped_address(xor_data, test_transaction_id);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first, test_ip);
    EXPECT_EQ(result->second, test_port);
}

TEST_F(StunMessageTest, ErrorResponse) {
    auto error_msg = create_error_response(
        test_transaction_id,
        StunErrorCode::BadRequest,
        "Test error"
    );

    EXPECT_EQ(error_msg.message_type(), StunMessageType::BindingErrorResponse);
    EXPECT_EQ(error_msg.transaction_id(), test_transaction_id);

    auto error_attr = error_msg.get_attribute(StunAttributeType::ErrorCode);
    ASSERT_TRUE(error_attr.has_value());
}

TEST_F(StunMessageTest, InvalidMessage) {
    uint8_t invalid_data[] = {0x00, 0x01, 0x00, 0x00};

    auto parsed = StunMessage::parse(invalid_data, sizeof(invalid_data));

    EXPECT_FALSE(parsed.has_value());
}

TEST_F(StunMessageTest, XorMappedAddressIPv6) {
    std::string test_ip = "2001:db8::1";
    uint16_t test_port = 54321;

    auto xor_data = create_xor_mapped_address(test_ip, test_port, test_transaction_id);

    ASSERT_FALSE(xor_data.empty());
    EXPECT_GT(xor_data.size(), 8);  // IPv6 should be larger than IPv4

    // Parse back
    auto result = parse_xor_mapped_address(xor_data, test_transaction_id);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first, test_ip);
    EXPECT_EQ(result->second, test_port);
}

TEST_F(StunMessageTest, MultipleAttributes) {
    StunMessage message(StunMessageType::BindingResponse, test_transaction_id);

    // Add multiple attributes
    std::vector<uint8_t> attr1_value = {0x01, 0x02, 0x03, 0x04};
    std::vector<uint8_t> attr2_value = {0x05, 0x06, 0x07, 0x08};

    message.add_attribute(StunAttribute(StunAttributeType::MappedAddress, attr1_value));
    message.add_attribute(StunAttribute(StunAttributeType::XorMappedAddress, attr2_value));

    auto data = message.serialize();

    // Parse back
    auto parsed = StunMessage::parse(data.data(), data.size());

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->attributes().size(), 2);

    auto attr1 = parsed->get_attribute(StunAttributeType::MappedAddress);
    ASSERT_TRUE(attr1.has_value());
    EXPECT_EQ((*attr1)->value, attr1_value);

    auto attr2 = parsed->get_attribute(StunAttributeType::XorMappedAddress);
    ASSERT_TRUE(attr2.has_value());
    EXPECT_EQ((*attr2)->value, attr2_value);
}

TEST_F(StunMessageTest, AttributePadding) {
    StunMessage message(StunMessageType::BindingResponse, test_transaction_id);

    // Add attribute with non-aligned length (should be padded to 4-byte boundary)
    std::vector<uint8_t> attr_value = {0x01, 0x02, 0x03};  // 3 bytes
    message.add_attribute(StunAttribute(StunAttributeType::MappedAddress, attr_value));

    auto data = message.serialize();

    // Parse back
    auto parsed = StunMessage::parse(data.data(), data.size());

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->attributes().size(), 1);

    auto attr = parsed->get_attribute(StunAttributeType::MappedAddress);
    ASSERT_TRUE(attr.has_value());
    EXPECT_EQ((*attr)->value, attr_value);
}

TEST_F(StunMessageTest, ErrorResponseWithReason) {
    std::string reason = "Bad Request - Invalid Message Format";

    auto error_msg = create_error_response(
        test_transaction_id,
        StunErrorCode::BadRequest,
        reason
    );

    EXPECT_EQ(error_msg.message_type(), StunMessageType::BindingErrorResponse);

    auto error_attr = error_msg.get_attribute(StunAttributeType::ErrorCode);
    ASSERT_TRUE(error_attr.has_value());

    // Check error code structure
    const auto& value = (*error_attr)->value;
    ASSERT_GE(value.size(), 4);

    uint8_t class_digit = value[2];
    uint8_t number = value[3];

    EXPECT_EQ(class_digit, 4);  // 400 error
    EXPECT_EQ(number, 0);
}

TEST_F(StunMessageTest, TransactionIdUniqueness) {
    TransactionId tid1, tid2;

    for (size_t i = 0; i < 12; ++i) {
        tid1[i] = static_cast<uint8_t>(i);
        tid2[i] = static_cast<uint8_t>(i + 1);
    }

    StunMessage msg1(StunMessageType::BindingRequest, tid1);
    StunMessage msg2(StunMessageType::BindingRequest, tid2);

    EXPECT_NE(msg1.transaction_id(), msg2.transaction_id());
}

TEST_F(StunMessageTest, EmptyMessage) {
    StunMessage message(StunMessageType::BindingRequest, test_transaction_id);

    auto data = message.serialize();

    // Should be exactly 20 bytes (header only)
    EXPECT_EQ(data.size(), 20);

    // Parse back
    auto parsed = StunMessage::parse(data.data(), data.size());

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->attributes().size(), 0);
}

TEST_F(StunMessageTest, LargeAttribute) {
    StunMessage message(StunMessageType::BindingResponse, test_transaction_id);

    // Create large attribute (100 bytes)
    std::vector<uint8_t> large_value(100);
    for (size_t i = 0; i < 100; ++i) {
        large_value[i] = static_cast<uint8_t>(i);
    }

    message.add_attribute(StunAttribute(StunAttributeType::Software, large_value));

    auto data = message.serialize();

    // Parse back
    auto parsed = StunMessage::parse(data.data(), data.size());

    ASSERT_TRUE(parsed.has_value());

    auto attr = parsed->get_attribute(StunAttributeType::Software);
    ASSERT_TRUE(attr.has_value());
    EXPECT_EQ((*attr)->value, large_value);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
