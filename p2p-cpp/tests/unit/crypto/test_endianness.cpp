#include <gtest/gtest.h>
#include "p2p/crypto/signed_envelope.hpp"
#include "p2p/crypto/ed25519_signer.hpp"
#include <vector>
#include <cstdint>

using namespace p2p::crypto;

class EndiannessTest : public ::testing::Test {
protected:
    void SetUp() override {
        private_key_ = Ed25519Signer::GeneratePrivateKey();
        public_key_ = Ed25519Signer::DerivePublicKey(private_key_);
    }

    Ed25519PrivateKey private_key_;
    Ed25519PublicKey public_key_;
};

TEST_F(EndiannessTest, SerializeDeserializeSymmetry) {
    std::string payload_type = "/libp2p/test";
    std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04, 0x05};

    auto envelope = SignedEnvelope::Sign(private_key_, payload_type, payload);

    auto serialized = envelope.Serialize();

    auto deserialized = SignedEnvelope::Deserialize(serialized);

    ASSERT_TRUE(deserialized.has_value());
    EXPECT_EQ(deserialized->payload_type, payload_type);
    EXPECT_EQ(deserialized->payload, payload);
    EXPECT_EQ(deserialized->public_key, envelope.public_key);
    EXPECT_EQ(deserialized->signature, envelope.signature);
}

TEST_F(EndiannessTest, SignatureValidAfterRoundTrip) {
    std::string payload_type = "/libp2p/relay-reservation";
    std::vector<uint8_t> payload = {0xDE, 0xAD, 0xBE, 0xEF};

    auto envelope = SignedEnvelope::Sign(private_key_, payload_type, payload);
    EXPECT_TRUE(envelope.Verify());

    auto serialized = envelope.Serialize();
    auto deserialized = SignedEnvelope::Deserialize(serialized);

    ASSERT_TRUE(deserialized.has_value());
    EXPECT_TRUE(deserialized->Verify());
}

TEST_F(EndiannessTest, BigEndianByteOrder) {
    std::string payload_type = "test";
    std::vector<uint8_t> payload = {0x01, 0x02};

    auto envelope = SignedEnvelope::Sign(private_key_, payload_type, payload);
    auto serialized = envelope.Serialize();

    ASSERT_GE(serialized.size(), 4u);

    uint32_t expected_len = 32;
    EXPECT_EQ(serialized[0], 0x00);
    EXPECT_EQ(serialized[1], 0x00);
    EXPECT_EQ(serialized[2], 0x00);
    EXPECT_EQ(serialized[3], 0x20);
}

TEST_F(EndiannessTest, CrossPlatformCompatibility) {
    std::string payload_type = "/libp2p/test";
    std::vector<uint8_t> payload = {0xCA, 0xFE, 0xBA, 0xBE};

    auto envelope1 = SignedEnvelope::Sign(private_key_, payload_type, payload);
    auto serialized = envelope1.Serialize();

    auto envelope2 = SignedEnvelope::Deserialize(serialized);

    ASSERT_TRUE(envelope2.has_value());
    EXPECT_EQ(envelope2->payload_type, envelope1.payload_type);
    EXPECT_EQ(envelope2->payload, envelope1.payload);
    EXPECT_TRUE(envelope2->Verify());
}

TEST_F(EndiannessTest, LargePayloadHandling) {
    std::string payload_type = "/libp2p/large-test";
    std::vector<uint8_t> payload(10000, 0xAB);

    auto envelope = SignedEnvelope::Sign(private_key_, payload_type, payload);
    auto serialized = envelope.Serialize();
    auto deserialized = SignedEnvelope::Deserialize(serialized);

    ASSERT_TRUE(deserialized.has_value());
    EXPECT_EQ(deserialized->payload.size(), 10000u);
    EXPECT_TRUE(deserialized->Verify());
}

TEST_F(EndiannessTest, EmptyPayloadHandling) {
    std::string payload_type = "/libp2p/empty";
    std::vector<uint8_t> payload;

    auto envelope = SignedEnvelope::Sign(private_key_, payload_type, payload);
    auto serialized = envelope.Serialize();
    auto deserialized = SignedEnvelope::Deserialize(serialized);

    ASSERT_TRUE(deserialized.has_value());
    EXPECT_TRUE(deserialized->payload.empty());
    EXPECT_TRUE(deserialized->Verify());
}

TEST_F(EndiannessTest, MultipleSerializationCycles) {
    std::string payload_type = "/libp2p/multi-cycle";
    std::vector<uint8_t> payload = {0x01, 0x02, 0x03};

    auto envelope1 = SignedEnvelope::Sign(private_key_, payload_type, payload);

    auto serialized1 = envelope1.Serialize();
    auto deserialized1 = SignedEnvelope::Deserialize(serialized1);
    ASSERT_TRUE(deserialized1.has_value());

    auto serialized2 = deserialized1->Serialize();
    auto deserialized2 = SignedEnvelope::Deserialize(serialized2);
    ASSERT_TRUE(deserialized2.has_value());

    auto serialized3 = deserialized2->Serialize();
    auto deserialized3 = SignedEnvelope::Deserialize(serialized3);
    ASSERT_TRUE(deserialized3.has_value());

    EXPECT_EQ(serialized1, serialized2);
    EXPECT_EQ(serialized2, serialized3);
    EXPECT_TRUE(deserialized3->Verify());
}

TEST_F(EndiannessTest, CorruptedLengthDetection) {
    std::string payload_type = "/libp2p/test";
    std::vector<uint8_t> payload = {0x01, 0x02};

    auto envelope = SignedEnvelope::Sign(private_key_, payload_type, payload);
    auto serialized = envelope.Serialize();

    serialized[0] = 0xFF;
    serialized[1] = 0xFF;
    serialized[2] = 0xFF;
    serialized[3] = 0xFF;

    auto deserialized = SignedEnvelope::Deserialize(serialized);
    EXPECT_FALSE(deserialized.has_value());
}

TEST_F(EndiannessTest, TruncatedDataDetection) {
    std::string payload_type = "/libp2p/test";
    std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04};

    auto envelope = SignedEnvelope::Sign(private_key_, payload_type, payload);
    auto serialized = envelope.Serialize();

    serialized.resize(serialized.size() / 2);

    auto deserialized = SignedEnvelope::Deserialize(serialized);
    EXPECT_FALSE(deserialized.has_value());
}

TEST_F(EndiannessTest, ByteOrderIndependence) {
    std::string payload_type = "/libp2p/test";
    std::vector<uint8_t> payload = {0x12, 0x34, 0x56, 0x78};

    auto envelope = SignedEnvelope::Sign(private_key_, payload_type, payload);
    auto serialized = envelope.Serialize();

    size_t offset = 0;

    uint32_t pk_len = (static_cast<uint32_t>(serialized[offset]) << 24) |
                      (static_cast<uint32_t>(serialized[offset + 1]) << 16) |
                      (static_cast<uint32_t>(serialized[offset + 2]) << 8) |
                      static_cast<uint32_t>(serialized[offset + 3]);
    EXPECT_EQ(pk_len, 32u);

    offset += 4 + pk_len;

    uint32_t pt_len = (static_cast<uint32_t>(serialized[offset]) << 24) |
                      (static_cast<uint32_t>(serialized[offset + 1]) << 16) |
                      (static_cast<uint32_t>(serialized[offset + 2]) << 8) |
                      static_cast<uint32_t>(serialized[offset + 3]);
    EXPECT_EQ(pt_len, payload_type.size());
}

TEST_F(EndiannessTest, TestVectorCompatibility) {
    std::string payload_type = "/libp2p/relay-reservation";
    std::vector<uint8_t> payload = {0x00, 0x01, 0x02, 0x03};

    auto envelope = SignedEnvelope::Sign(private_key_, payload_type, payload);
    auto serialized = envelope.Serialize();

    EXPECT_GE(serialized.size(), 4u + 32u + 4u + payload_type.size() + 4u + payload.size() + 4u + 64u);

    auto deserialized = SignedEnvelope::Deserialize(serialized);
    ASSERT_TRUE(deserialized.has_value());
    EXPECT_TRUE(deserialized->Verify());
}
