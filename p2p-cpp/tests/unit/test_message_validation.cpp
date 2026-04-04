#include <gtest/gtest.h>
#include "p2p/protocol/message.hpp"
#include <vector>
#include <cstring>

using namespace p2p::protocol;

class MessageValidationTest : public ::testing::Test {
protected:
    // Helper to create a valid message for testing
    std::vector<uint8_t> createValidMessage() {
        Message msg(MessageType::HANDSHAKE, "sender123", "receiver456");
        return msg.encode();
    }

    // Helper to create message with custom sizes
    std::vector<uint8_t> createMessageWithSize(uint32_t total_len, uint32_t json_len) {
        std::vector<uint8_t> data(8);

        // Write total_length (big-endian)
        data[0] = (total_len >> 24) & 0xFF;
        data[1] = (total_len >> 16) & 0xFF;
        data[2] = (total_len >> 8) & 0xFF;
        data[3] = total_len & 0xFF;

        // Write json_length (big-endian)
        data[4] = (json_len >> 24) & 0xFF;
        data[5] = (json_len >> 16) & 0xFF;
        data[6] = (json_len >> 8) & 0xFF;
        data[7] = json_len & 0xFF;

        return data;
    }
};

TEST_F(MessageValidationTest, RejectTooSmallMessage) {
    std::vector<uint8_t> data = {0x01, 0x02, 0x03};  // Only 3 bytes
    auto msg = Message::decode(data);
    EXPECT_EQ(msg, nullptr);
}

TEST_F(MessageValidationTest, RejectOversizedMessage) {
    constexpr size_t MAX_SIZE = 64 * 1024 + 1;  // 64KB + 1
    std::vector<uint8_t> data(MAX_SIZE, 0xFF);
    auto msg = Message::decode(data);
    EXPECT_EQ(msg, nullptr);
}

TEST_F(MessageValidationTest, RejectOversizedTotalLength) {
    auto data = createMessageWithSize(65 * 1024, 100);  // 65KB total
    data.resize(8 + 100);  // Add some data
    auto msg = Message::decode(data);
    EXPECT_EQ(msg, nullptr);
}

TEST_F(MessageValidationTest, RejectOversizedJsonLength) {
    auto data = createMessageWithSize(100, 17 * 1024);  // 17KB JSON
    data.resize(8 + 100);
    auto msg = Message::decode(data);
    EXPECT_EQ(msg, nullptr);
}

TEST_F(MessageValidationTest, RejectInvalidLengthRelation) {
    auto data = createMessageWithSize(100, 200);  // json > total
    data.resize(8 + 100);
    auto msg = Message::decode(data);
    EXPECT_EQ(msg, nullptr);
}

TEST_F(MessageValidationTest, RejectOversizedPayload) {
    auto data = createMessageWithSize(61 * 1024, 100);  // 61KB payload
    data.resize(8 + 61 * 1024);
    auto msg = Message::decode(data);
    EXPECT_EQ(msg, nullptr);
}

TEST_F(MessageValidationTest, RejectMalformedJson) {
    auto data = createMessageWithSize(20, 10);
    data.resize(8 + 20);
    // Add invalid JSON
    std::string invalid_json = "not json!";
    std::copy(invalid_json.begin(), invalid_json.end(), data.begin() + 8);

    auto msg = Message::decode(data);
    EXPECT_EQ(msg, nullptr);
}

TEST_F(MessageValidationTest, RejectMissingRequiredFields) {
    auto data = createMessageWithSize(100, 50);
    data.resize(8 + 100);

    // JSON missing sender_did
    std::string incomplete_json = R"({
        "msg_type": 1,
        "receiver_did": "receiver",
        "message_id": "msg123",
        "timestamp": 1234567890
    })";

    std::copy(incomplete_json.begin(), incomplete_json.end(), data.begin() + 8);

    auto msg = Message::decode(data);
    EXPECT_EQ(msg, nullptr);
}

TEST_F(MessageValidationTest, RejectInvalidMessageType) {
    auto data = createMessageWithSize(200, 150);
    data.resize(8 + 200);

    // Invalid message type (0x99)
    std::string json_str = R"({
        "msg_type": 153,
        "sender_did": "sender",
        "receiver_did": "receiver",
        "message_id": "msg123",
        "timestamp": 1234567890,
        "channel_id": null
    })";

    std::copy(json_str.begin(), json_str.end(), data.begin() + 8);

    auto msg = Message::decode(data);
    EXPECT_EQ(msg, nullptr);
}

TEST_F(MessageValidationTest, RejectEmptyDid) {
    auto data = createMessageWithSize(200, 150);
    data.resize(8 + 200);

    std::string json_str = R"({
        "msg_type": 1,
        "sender_did": "",
        "receiver_did": "receiver",
        "message_id": "msg123",
        "timestamp": 1234567890,
        "channel_id": null
    })";

    std::copy(json_str.begin(), json_str.end(), data.begin() + 8);

    auto msg = Message::decode(data);
    EXPECT_EQ(msg, nullptr);
}

// Test: Reject oversized DID
TEST_F(MessageValidationTest, RejectOversizedDid) {
    auto data = createMessageWithSize(500, 400);
    data.resize(8 + 500);

    std::string long_did(300, 'x');  // 300 characters
    std::string json_str = R"({
        "msg_type": 1,
        "sender_did": ")" + long_did + R"(",
        "receiver_did": "receiver",
        "message_id": "msg123",
        "timestamp": 1234567890,
        "channel_id": null
    })";

    std::copy(json_str.begin(), json_str.end(), data.begin() + 8);

    auto msg = Message::decode(data);
    EXPECT_EQ(msg, nullptr);
}

TEST_F(MessageValidationTest, RejectInvalidChannelId) {
    auto data = createMessageWithSize(200, 150);
    data.resize(8 + 200);

    // channel_id = 70000 (exceeds 65535)
    std::string json_str = R"({
        "msg_type": 1,
        "sender_did": "sender",
        "receiver_did": "receiver",
        "message_id": "msg123",
        "timestamp": 1234567890,
        "channel_id": 70000
    })";

    std::copy(json_str.begin(), json_str.end(), data.begin() + 8);

    auto msg = Message::decode(data);
    EXPECT_EQ(msg, nullptr);
}

// Test: Reject too many metadata entries
TEST_F(MessageValidationTest, RejectTooManyMetadataEntries) {
    auto data = createMessageWithSize(2000, 1800);
    data.resize(8 + 2000);

    // Create JSON with 40 metadata entries (exceeds limit of 32)
    std::string json_str = R"({
        "msg_type": 1,
        "sender_did": "sender",
        "receiver_did": "receiver",
        "message_id": "msg123",
        "timestamp": 1234567890,
        "channel_id": null,
        "metadata": {)";

    for (int i = 0; i < 40; ++i) {
        json_str += "\"key" + std::to_string(i) + "\": \"value" + std::to_string(i) + "\"";
        if (i < 39) json_str += ",";
    }
    json_str += "}}";

    std::copy(json_str.begin(), json_str.end(), data.begin() + 8);

    auto msg = Message::decode(data);
    EXPECT_EQ(msg, nullptr);
}

// Test: Accept valid message
TEST_F(MessageValidationTest, AcceptValidMessage) {
    auto data = createValidMessage();
    auto msg = Message::decode(data);
    EXPECT_NE(msg, nullptr);
    EXPECT_EQ(msg->type(), MessageType::HANDSHAKE);
    EXPECT_EQ(msg->sender_did(), "sender123");
    EXPECT_EQ(msg->receiver_did(), "receiver456");
}

// Test: Accept message with valid metadata
TEST_F(MessageValidationTest, AcceptValidMetadata) {
    Message msg(MessageType::HANDSHAKE, "sender", "receiver");
    msg.add_metadata("key1", "value1");
    msg.add_metadata("key2", "value2");

    auto encoded = msg.encode();
    auto decoded = Message::decode(encoded);

    EXPECT_NE(decoded, nullptr);
    EXPECT_EQ(decoded->metadata().size(), 2u);
    EXPECT_EQ(decoded->metadata().at("key1"), "value1");
    EXPECT_EQ(decoded->metadata().at("key2"), "value2");
}

// Test: Accept message with valid channel_id
TEST_F(MessageValidationTest, AcceptValidChannelId) {
    Message msg(MessageType::CHANNEL_DATA, "sender", "receiver");
    msg.set_channel_id(42);

    auto encoded = msg.encode();
    auto decoded = Message::decode(encoded);

    EXPECT_NE(decoded, nullptr);
    EXPECT_TRUE(decoded->channel_id().has_value());
    EXPECT_EQ(decoded->channel_id().value(), 42);
}
