#include <gtest/gtest.h>
#include "p2p/protocol/message.hpp"

using namespace p2p::protocol;

class MessageTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(MessageTest, BasicMessageEncodeDecode) {
    // Create a message
    Message msg(MessageType::KEEPALIVE, "sender_123", "receiver_456");

    // Encode
    auto encoded = msg.encode();
    EXPECT_GT(encoded.size(), 0);

    // Decode
    auto decoded = Message::decode(encoded);
    ASSERT_NE(decoded, nullptr);

    // Verify
    EXPECT_EQ(decoded->type(), MessageType::KEEPALIVE);
    EXPECT_EQ(decoded->sender_did(), "sender_123");
    EXPECT_EQ(decoded->receiver_did(), "receiver_456");
}

TEST_F(MessageTest, HandshakeMessage) {
    HandshakeMessage msg("sender_123", "receiver_456", false);
    msg.set_public_address("192.168.1.100", 12345);
    msg.set_nat_type("full_cone");
    msg.add_capability("video");
    msg.add_capability("audio");

    // Encode and decode
    auto encoded = msg.encode();
    auto decoded = Message::decode(encoded);
    ASSERT_NE(decoded, nullptr);

    // Verify type
    EXPECT_EQ(decoded->type(), MessageType::HANDSHAKE);
}

TEST_F(MessageTest, ChannelDataMessage) {
    std::vector<uint8_t> payload = {1, 2, 3, 4, 5};
    ChannelDataMessage msg("sender_123", "receiver_456", 42, payload);

    // Encode and decode
    auto encoded = msg.encode();
    auto decoded = Message::decode(encoded);
    ASSERT_NE(decoded, nullptr);

    // Verify
    EXPECT_EQ(decoded->type(), MessageType::CHANNEL_DATA);
    EXPECT_TRUE(decoded->channel_id().has_value());
    EXPECT_EQ(decoded->channel_id().value(), 42);
    EXPECT_EQ(decoded->payload(), payload);
}

TEST_F(MessageTest, DisconnectMessage) {
    DisconnectMessage msg("sender_123", "receiver_456", "User requested");

    // Encode and decode
    auto encoded = msg.encode();
    auto decoded = Message::decode(encoded);
    ASSERT_NE(decoded, nullptr);

    // Verify
    EXPECT_EQ(decoded->type(), MessageType::DISCONNECT);
    EXPECT_EQ(decoded->metadata().at("reason"), "User requested");
}

TEST_F(MessageTest, InvalidData) {
    std::vector<uint8_t> invalid_data = {1, 2, 3};  // Too short
    auto decoded = Message::decode(invalid_data);
    EXPECT_EQ(decoded, nullptr);
}

TEST_F(MessageTest, LargePayload) {
    std::vector<uint8_t> large_payload(10000, 0xAB);
    ChannelDataMessage msg("sender_123", "receiver_456", 1, large_payload);

    // Encode and decode
    auto encoded = msg.encode();
    auto decoded = Message::decode(encoded);
    ASSERT_NE(decoded, nullptr);

    // Verify payload
    EXPECT_EQ(decoded->payload().size(), 10000);
    EXPECT_EQ(decoded->payload()[0], 0xAB);
}

TEST_F(MessageTest, MessageIDUniqueness) {
    Message msg1(MessageType::KEEPALIVE, "sender_123", "receiver_456");
    Message msg2(MessageType::KEEPALIVE, "sender_123", "receiver_456");

    // Message IDs should be unique
    EXPECT_NE(msg1.message_id(), msg2.message_id());
}

TEST_F(MessageTest, TimestampGeneration) {
    Message msg(MessageType::KEEPALIVE, "sender_123", "receiver_456");

    // Timestamp should be non-zero
    EXPECT_GT(msg.timestamp(), 0);

    // Timestamp should be recent (within last minute)
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    uint64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

    EXPECT_LE(msg.timestamp(), now_ms);
    EXPECT_GT(msg.timestamp(), now_ms - 60000);  // Within last minute
}

TEST_F(MessageTest, MetadataHandling) {
    Message msg(MessageType::KEEPALIVE, "sender_123", "receiver_456");

    // Add metadata
    msg.add_metadata("key1", "value1");
    msg.add_metadata("key2", "value2");

    // Encode and decode
    auto encoded = msg.encode();
    auto decoded = Message::decode(encoded);
    ASSERT_NE(decoded, nullptr);

    // Verify metadata
    EXPECT_EQ(decoded->metadata().at("key1"), "value1");
    EXPECT_EQ(decoded->metadata().at("key2"), "value2");
}

TEST_F(MessageTest, EmptyPayload) {
    ChannelDataMessage msg("sender_123", "receiver_456", 1, {});

    // Encode and decode
    auto encoded = msg.encode();
    auto decoded = Message::decode(encoded);
    ASSERT_NE(decoded, nullptr);

    // Verify empty payload
    EXPECT_TRUE(decoded->payload().empty());
}

TEST_F(MessageTest, HandshakeAckMessage) {
    HandshakeMessage msg("sender_123", "receiver_456", true);

    // Encode and decode
    auto encoded = msg.encode();
    auto decoded = Message::decode(encoded);
    ASSERT_NE(decoded, nullptr);

    // Verify type is HANDSHAKE_ACK
    EXPECT_EQ(decoded->type(), MessageType::HANDSHAKE_ACK);
    EXPECT_EQ(decoded->metadata().at("is_ack"), "true");
}

TEST_F(MessageTest, HandshakeAddressExtraction) {
    HandshakeMessage msg("sender_123", "receiver_456", false);
    msg.set_public_address("203.0.113.1", 54321);
    msg.set_local_address("192.168.1.100", 12345);

    // Encode and decode
    auto encoded = msg.encode();
    auto decoded = Message::decode(encoded);
    ASSERT_NE(decoded, nullptr);

    // Verify addresses in metadata
    EXPECT_EQ(decoded->metadata().at("public_ip"), "203.0.113.1");
    EXPECT_EQ(decoded->metadata().at("public_port"), "54321");
    EXPECT_EQ(decoded->metadata().at("local_ip"), "192.168.1.100");
    EXPECT_EQ(decoded->metadata().at("local_port"), "12345");
}

TEST_F(MessageTest, HandshakeCapabilities) {
    HandshakeMessage msg("sender_123", "receiver_456", false);
    msg.add_capability("video");
    msg.add_capability("audio");
    msg.add_capability("screen_share");

    // Encode and decode
    auto encoded = msg.encode();
    auto decoded = Message::decode(encoded);
    ASSERT_NE(decoded, nullptr);

    // Verify capabilities
    EXPECT_EQ(decoded->metadata().at("capabilities"), "video,audio,screen_share");
}

TEST_F(MessageTest, HandshakeNATType) {
    HandshakeMessage msg("sender_123", "receiver_456", false);
    msg.set_nat_type("symmetric");

    // Encode and decode
    auto encoded = msg.encode();
    auto decoded = Message::decode(encoded);
    ASSERT_NE(decoded, nullptr);

    // Verify NAT type
    EXPECT_EQ(decoded->metadata().at("nat_type"), "symmetric");
}

TEST_F(MessageTest, ChannelIDHandling) {
    Message msg(MessageType::CHANNEL_DATA, "sender_123", "receiver_456");
    msg.set_channel_id(99);

    // Encode and decode
    auto encoded = msg.encode();
    auto decoded = Message::decode(encoded);
    ASSERT_NE(decoded, nullptr);

    // Verify channel ID
    EXPECT_TRUE(decoded->channel_id().has_value());
    EXPECT_EQ(decoded->channel_id().value(), 99);
}

TEST_F(MessageTest, NoChannelID) {
    Message msg(MessageType::KEEPALIVE, "sender_123", "receiver_456");

    // Encode and decode
    auto encoded = msg.encode();
    auto decoded = Message::decode(encoded);
    ASSERT_NE(decoded, nullptr);

    // Verify no channel ID
    EXPECT_FALSE(decoded->channel_id().has_value());
}

TEST_F(MessageTest, DisconnectWithoutReason) {
    DisconnectMessage msg("sender_123", "receiver_456", "");

    // Encode and decode
    auto encoded = msg.encode();
    auto decoded = Message::decode(encoded);
    ASSERT_NE(decoded, nullptr);

    // Verify type
    EXPECT_EQ(decoded->type(), MessageType::DISCONNECT);
}

TEST_F(MessageTest, CorruptedJSON) {
    // Create valid message
    Message msg(MessageType::KEEPALIVE, "sender_123", "receiver_456");
    auto encoded = msg.encode();

    // Corrupt JSON part
    if (encoded.size() > 20) {
        encoded[15] = 0xFF;  // Corrupt byte
    }

    // Decode should fail gracefully
    auto decoded = Message::decode(encoded);
    EXPECT_EQ(decoded, nullptr);
}

TEST_F(MessageTest, TruncatedMessage) {
    Message msg(MessageType::KEEPALIVE, "sender_123", "receiver_456");
    auto encoded = msg.encode();

    // Truncate message
    if (encoded.size() > 10) {
        encoded.resize(10);
    }

    // Decode should fail
    auto decoded = Message::decode(encoded);
    EXPECT_EQ(decoded, nullptr);
}

TEST_F(MessageTest, BinaryPayload) {
    // Create binary payload with all byte values
    std::vector<uint8_t> binary_payload;
    for (int i = 0; i < 256; ++i) {
        binary_payload.push_back(static_cast<uint8_t>(i));
    }

    ChannelDataMessage msg("sender_123", "receiver_456", 1, binary_payload);

    // Encode and decode
    auto encoded = msg.encode();
    auto decoded = Message::decode(encoded);
    ASSERT_NE(decoded, nullptr);

    // Verify binary payload
    EXPECT_EQ(decoded->payload(), binary_payload);
}

TEST_F(MessageTest, MaxChannelID) {
    // Channel ID is limited to 0-65535 (16-bit unsigned) as per protocol spec
    constexpr int MAX_VALID_CHANNEL_ID = 65535;
    ChannelDataMessage msg("sender_123", "receiver_456", MAX_VALID_CHANNEL_ID, {1, 2, 3});

    // Encode and decode
    auto encoded = msg.encode();
    auto decoded = Message::decode(encoded);
    ASSERT_NE(decoded, nullptr);

    // Verify max valid channel ID
    EXPECT_TRUE(decoded->channel_id().has_value());
    EXPECT_EQ(decoded->channel_id().value(), MAX_VALID_CHANNEL_ID);
}

TEST_F(MessageTest, LongDIDStrings) {
    // DID length is limited to 256 characters as per protocol spec for security
    constexpr size_t MAX_DID_LENGTH = 256;
    std::string max_did(MAX_DID_LENGTH, 'x');
    Message msg(MessageType::KEEPALIVE, max_did, max_did);

    // Encode and decode
    auto encoded = msg.encode();
    auto decoded = Message::decode(encoded);
    ASSERT_NE(decoded, nullptr);

    // Verify max length DIDs
    EXPECT_EQ(decoded->sender_did(), max_did);
    EXPECT_EQ(decoded->receiver_did(), max_did);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
