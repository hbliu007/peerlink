#include "p2p/protocol/message.hpp"
#include <gtest/gtest.h>
#include <vector>
#include <cstring>

using namespace p2p::protocol;

TEST(MessageCodecTest, BasicEncodeDecodeRoundTrip) {
    Message msg(MessageType::KEEPALIVE, "sender-did", "receiver-did");

    auto encoded = msg.encode();
    EXPECT_GT(encoded.size(), 8);  // At least header

    auto decoded = Message::decode(encoded);
    ASSERT_NE(decoded, nullptr);
    EXPECT_EQ(decoded->type(), MessageType::KEEPALIVE);
    EXPECT_EQ(decoded->sender_did(), "sender-did");
    EXPECT_EQ(decoded->receiver_did(), "receiver-did");
    EXPECT_EQ(decoded->payload().size(), 0);
}

TEST(MessageCodecTest, MessageWithPayload) {
    ChannelDataMessage msg("alice", "bob", 42, {0x01, 0x02, 0x03, 0x04, 0x05});

    auto encoded = msg.encode();
    auto decoded = Message::decode(encoded);

    ASSERT_NE(decoded, nullptr);
    EXPECT_EQ(decoded->type(), MessageType::CHANNEL_DATA);
    EXPECT_EQ(decoded->sender_did(), "alice");
    EXPECT_EQ(decoded->receiver_did(), "bob");
    EXPECT_EQ(decoded->channel_id().value(), 42);

    auto payload = decoded->payload();
    ASSERT_EQ(payload.size(), 5);
    EXPECT_EQ(payload[0], 0x01);
    EXPECT_EQ(payload[4], 0x05);
}

TEST(MessageCodecTest, MessageWithLargePayload) {
    std::vector<uint8_t> large_payload(10000);
    for (size_t i = 0; i < large_payload.size(); ++i) {
        large_payload[i] = static_cast<uint8_t>(i % 256);
    }

    ChannelDataMessage msg("sender", "receiver", 1, large_payload);

    auto encoded = msg.encode();
    auto decoded = Message::decode(encoded);

    ASSERT_NE(decoded, nullptr);
    EXPECT_EQ(decoded->payload(), large_payload);
}

TEST(MessageCodecTest, MessageWithMetadata) {
    Message msg(MessageType::HANDSHAKE, "peer1", "peer2");
    msg.add_metadata("key1", "value1");
    msg.add_metadata("key2", "value2");

    auto encoded = msg.encode();
    auto decoded = Message::decode(encoded);

    ASSERT_NE(decoded, nullptr);
    auto metadata = decoded->metadata();
    EXPECT_EQ(metadata.size(), 2);
    EXPECT_EQ(metadata.at("key1"), "value1");
    EXPECT_EQ(metadata.at("key2"), "value2");
}

// H-4 修复验证：粘包场景下 payload 边界正确
TEST(MessageCodecTest, PayloadBoundaryWithStickyPacket_H4Fix) {
    // 创建两条消息
    ChannelDataMessage msg1("alice", "bob", 1, {0xAA, 0xBB, 0xCC});
    ChannelDataMessage msg2("bob", "alice", 2, {0x11, 0x22, 0x33, 0x44});

    auto frame1 = msg1.encode();
    auto frame2 = msg2.encode();

    // 模拟 TCP 粘包：两条消息连在一起
    std::vector<uint8_t> sticky_packet;
    sticky_packet.insert(sticky_packet.end(), frame1.begin(), frame1.end());
    sticky_packet.insert(sticky_packet.end(), frame2.begin(), frame2.end());

    // decode 第一条消息时，应该只提取第一条消息的 payload
    // 不应该包含第二条消息的数据（这是 H-4 修复的关键）
    auto decoded1 = Message::decode(sticky_packet);

    ASSERT_NE(decoded1, nullptr);
    EXPECT_EQ(decoded1->sender_did(), "alice");
    EXPECT_EQ(decoded1->receiver_did(), "bob");
    EXPECT_EQ(decoded1->channel_id().value(), 1);

    // 关键验证：payload 应该只包含第一条消息的数据
    auto payload1 = decoded1->payload();
    ASSERT_EQ(payload1.size(), 3);
    EXPECT_EQ(payload1[0], 0xAA);
    EXPECT_EQ(payload1[1], 0xBB);
    EXPECT_EQ(payload1[2], 0xCC);

    // 不应该包含第二条消息的数据
    // 如果 H-4 bug 存在，payload 会包含 {0xAA, 0xBB, 0xCC, ...第二条消息的所有字节}
}

TEST(MessageCodecTest, EmptyPayload) {
    ChannelDataMessage msg("sender", "receiver", 5, {});

    auto encoded = msg.encode();
    auto decoded = Message::decode(encoded);

    ASSERT_NE(decoded, nullptr);
    EXPECT_EQ(decoded->payload().size(), 0);
}

TEST(MessageCodecTest, DecodeIncompleteFrame) {
    Message msg(MessageType::KEEPALIVE, "alice", "bob");
    auto encoded = msg.encode();

    // 截断帧（只保留一半）
    std::vector<uint8_t> incomplete(encoded.begin(), encoded.begin() + encoded.size() / 2);

    auto decoded = Message::decode(incomplete);
    EXPECT_EQ(decoded, nullptr);  // 应该返回 nullptr
}

TEST(MessageCodecTest, DecodeInvalidHeader) {
    // 创建一个非法的 header（total_length 太小）
    std::vector<uint8_t> bad_frame = {
        0x00, 0x00, 0x00, 0x05,  // total_length = 5 (太小，无法包含 json)
        0x00, 0x00, 0x00, 0x10,  // json_length = 16 (大于 total_length)
        0x00, 0x00, 0x00, 0x00
    };

    auto decoded = Message::decode(bad_frame);
    EXPECT_EQ(decoded, nullptr);
}

TEST(MessageCodecTest, DecodeOversizedFrame) {
    // 创建一个声称超大的 header
    std::vector<uint8_t> huge_frame = {
        0x10, 0x00, 0x00, 0x00,  // total_length = 256MB (超过 MAX_PAYLOAD_SIZE)
        0x00, 0x00, 0x00, 0x10,
        0x00, 0x00, 0x00, 0x00
    };

    auto decoded = Message::decode(huge_frame);
    EXPECT_EQ(decoded, nullptr);
}

TEST(MessageCodecTest, DecodeInvalidJSON) {
    // 手工构造一个帧，但 JSON 部分是非法的
    std::vector<uint8_t> bad_json_frame;

    uint32_t total_length = 10;
    uint32_t json_length = 10;

    // Header
    bad_json_frame.push_back((total_length >> 24) & 0xFF);
    bad_json_frame.push_back((total_length >> 16) & 0xFF);
    bad_json_frame.push_back((total_length >> 8) & 0xFF);
    bad_json_frame.push_back(total_length & 0xFF);

    bad_json_frame.push_back((json_length >> 24) & 0xFF);
    bad_json_frame.push_back((json_length >> 16) & 0xFF);
    bad_json_frame.push_back((json_length >> 8) & 0xFF);
    bad_json_frame.push_back(json_length & 0xFF);

    // 非法 JSON（不是有效的 JSON 字符串）
    for (int i = 0; i < 10; ++i) {
        bad_json_frame.push_back(0xFF);
    }

    auto decoded = Message::decode(bad_json_frame);
    EXPECT_EQ(decoded, nullptr);
}

TEST(MessageCodecTest, DecodeMissingRequiredFields) {
    // 手工构造一个帧，JSON 缺少必需字段
    std::vector<uint8_t> incomplete_json_frame;

    std::string json = R"({"msg_type":3})";  // 缺少 sender_did, receiver_did 等
    uint32_t total_length = json.size();
    uint32_t json_length = json.size();

    // Header
    incomplete_json_frame.push_back((total_length >> 24) & 0xFF);
    incomplete_json_frame.push_back((total_length >> 16) & 0xFF);
    incomplete_json_frame.push_back((total_length >> 8) & 0xFF);
    incomplete_json_frame.push_back(total_length & 0xFF);

    incomplete_json_frame.push_back((json_length >> 24) & 0xFF);
    incomplete_json_frame.push_back((json_length >> 16) & 0xFF);
    incomplete_json_frame.push_back((json_length >> 8) & 0xFF);
    incomplete_json_frame.push_back(json_length & 0xFF);

    // JSON
    incomplete_json_frame.insert(incomplete_json_frame.end(), json.begin(), json.end());

    auto decoded = Message::decode(incomplete_json_frame);
    EXPECT_EQ(decoded, nullptr);
}

TEST(MessageCodecTest, HandshakeMessageRoundTrip) {
    HandshakeMessage msg("peer1", "peer2", false);
    msg.set_public_address("192.168.1.100", 5000);
    msg.set_local_address("10.0.0.5", 6000);
    msg.set_nat_type("full-cone");

    auto encoded = msg.encode();
    auto decoded = Message::decode(encoded);

    ASSERT_NE(decoded, nullptr);
    EXPECT_EQ(decoded->type(), MessageType::HANDSHAKE);

    auto metadata = decoded->metadata();
    EXPECT_EQ(metadata.at("public_ip"), "192.168.1.100");
    EXPECT_EQ(metadata.at("public_port"), "5000");
    EXPECT_EQ(metadata.at("local_ip"), "10.0.0.5");
    EXPECT_EQ(metadata.at("local_port"), "6000");
    EXPECT_EQ(metadata.at("nat_type"), "full-cone");
}

TEST(MessageCodecTest, DisconnectMessageRoundTrip) {
    DisconnectMessage msg("alice", "bob", "user requested");

    auto encoded = msg.encode();
    auto decoded = Message::decode(encoded);

    ASSERT_NE(decoded, nullptr);
    EXPECT_EQ(decoded->type(), MessageType::DISCONNECT);
    EXPECT_EQ(decoded->sender_did(), "alice");
    EXPECT_EQ(decoded->receiver_did(), "bob");

    auto metadata = decoded->metadata();
    EXPECT_EQ(metadata.at("reason"), "user requested");
}
