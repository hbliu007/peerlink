#include <gtest/gtest.h>
#include "p2p/crypto/signed_envelope.hpp"
#include <vector>
#include <string>

using namespace p2p::crypto;

class SignedEnvelopeTest : public ::testing::Test {
protected:
    void SetUp() override {
        private_key_ = Ed25519Signer::GenerateKeyPair();
        public_key_ = Ed25519Signer::DerivePublicKey(private_key_);

        payload_type_ = "/libp2p/relay-reservation";
        payload_ = {0x01, 0x02, 0x03, 0x04, 0x05};
    }

    Ed25519PrivateKey private_key_;
    Ed25519PublicKey public_key_;
    std::string payload_type_;
    std::vector<uint8_t> payload_;
};

TEST_F(SignedEnvelopeTest, SignAndVerify) {
    SignedEnvelope envelope = SignedEnvelope::Sign(
        private_key_,
        payload_type_,
        payload_
    );

    EXPECT_EQ(envelope.public_key.size(), Ed25519PublicKey::KEY_SIZE);
    EXPECT_EQ(envelope.payload_type, payload_type_);
    EXPECT_EQ(envelope.payload, payload_);
    EXPECT_EQ(envelope.signature.size(), Ed25519Signature::SIGNATURE_SIZE);

    EXPECT_TRUE(envelope.Verify());
}

TEST_F(SignedEnvelopeTest, VerifyWithType) {
    SignedEnvelope envelope = SignedEnvelope::Sign(
        private_key_,
        payload_type_,
        payload_
    );

    EXPECT_TRUE(envelope.VerifyWithType(payload_type_));

    EXPECT_FALSE(envelope.VerifyWithType("/wrong/type"));
}

TEST_F(SignedEnvelopeTest, VerifyInvalidSignature) {
    SignedEnvelope envelope = SignedEnvelope::Sign(
        private_key_,
        payload_type_,
        payload_
    );

    envelope.signature[0] ^= 0xFF;

    EXPECT_FALSE(envelope.Verify());
}

TEST_F(SignedEnvelopeTest, VerifyModifiedPayload) {
    SignedEnvelope envelope = SignedEnvelope::Sign(
        private_key_,
        payload_type_,
        payload_
    );

    envelope.payload[0] ^= 0xFF;

    EXPECT_FALSE(envelope.Verify());
}

TEST_F(SignedEnvelopeTest, VerifyModifiedPayloadType) {
    SignedEnvelope envelope = SignedEnvelope::Sign(
        private_key_,
        payload_type_,
        payload_
    );

    envelope.payload_type = "/modified/type";

    EXPECT_FALSE(envelope.Verify());
}

TEST_F(SignedEnvelopeTest, SerializeAndDeserialize) {
    SignedEnvelope envelope = SignedEnvelope::Sign(
        private_key_,
        payload_type_,
        payload_
    );

    std::vector<uint8_t> serialized = envelope.Serialize();
    EXPECT_GT(serialized.size(), 0u);

    auto deserialized = SignedEnvelope::Deserialize(serialized);
    ASSERT_TRUE(deserialized.has_value());

    EXPECT_EQ(deserialized->public_key, envelope.public_key);
    EXPECT_EQ(deserialized->payload_type, envelope.payload_type);
    EXPECT_EQ(deserialized->payload, envelope.payload);
    EXPECT_EQ(deserialized->signature, envelope.signature);

    EXPECT_TRUE(deserialized->Verify());
}

TEST_F(SignedEnvelopeTest, DeserializeInvalidData) {
    std::vector<uint8_t> invalid_data = {0x01, 0x02, 0x03};

    auto result = SignedEnvelope::Deserialize(invalid_data);
    EXPECT_FALSE(result.has_value());
}

TEST_F(SignedEnvelopeTest, DeserializeEmptyData) {
    std::vector<uint8_t> empty_data;

    auto result = SignedEnvelope::Deserialize(empty_data);
    EXPECT_FALSE(result.has_value());
}

TEST_F(SignedEnvelopeTest, SignEmptyPayload) {
    std::vector<uint8_t> empty_payload;
    SignedEnvelope envelope = SignedEnvelope::Sign(
        private_key_,
        payload_type_,
        empty_payload
    );

    EXPECT_TRUE(envelope.Verify());
    EXPECT_EQ(envelope.payload.size(), 0u);
}

TEST_F(SignedEnvelopeTest, SignLargePayload) {
    std::vector<uint8_t> large_payload(1024 * 1024, 0xAB);

    SignedEnvelope envelope = SignedEnvelope::Sign(
        private_key_,
        payload_type_,
        large_payload
    );

    EXPECT_TRUE(envelope.Verify());
}

TEST_F(SignedEnvelopeTest, DomainStringSeparation) {
    std::string type1 = "/type1";
    std::string type2 = "/type2";

    SignedEnvelope envelope1 = SignedEnvelope::Sign(private_key_, type1, payload_);
    SignedEnvelope envelope2 = SignedEnvelope::Sign(private_key_, type2, payload_);

    EXPECT_NE(envelope1.signature, envelope2.signature);

    EXPECT_TRUE(envelope1.VerifyWithType(type1));
    EXPECT_TRUE(envelope2.VerifyWithType(type2));

    EXPECT_FALSE(envelope1.VerifyWithType(type2));
    EXPECT_FALSE(envelope2.VerifyWithType(type1));
}

TEST_F(SignedEnvelopeTest, RoundTripSerialization) {
    SignedEnvelope original = SignedEnvelope::Sign(
        private_key_,
        payload_type_,
        payload_
    );

    for (int i = 0; i < 3; ++i) {
        std::vector<uint8_t> serialized = original.Serialize();
        auto deserialized = SignedEnvelope::Deserialize(serialized);

        ASSERT_TRUE(deserialized.has_value());
        EXPECT_TRUE(deserialized->Verify());

        original = *deserialized;
    }
}
