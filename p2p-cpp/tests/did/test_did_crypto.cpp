#include <gtest/gtest.h>
#include "servers/did/did_crypto.hpp"

using namespace p2p::did;

class DIDCryptoTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Generate a keypair once for reuse in sign/verify tests
        keypair_ = DIDCrypto::GenerateKeyPair();
        // Extract private and public key PEM blocks
        auto pub_pos = keypair_.find("-----BEGIN PUBLIC KEY-----");
        if (pub_pos != std::string::npos) {
            private_key_ = keypair_.substr(0, pub_pos);
            public_key_ = keypair_.substr(pub_pos);
        }
    }

    std::string keypair_;
    std::string private_key_;
    std::string public_key_;
};

TEST_F(DIDCryptoTest, GenerateKeyPair) {
    EXPECT_FALSE(keypair_.empty());
    EXPECT_NE(keypair_.find("-----BEGIN PRIVATE KEY-----"), std::string::npos);
    EXPECT_NE(keypair_.find("-----BEGIN PUBLIC KEY-----"), std::string::npos);
}

TEST_F(DIDCryptoTest, GenerateMultipleKeyPairs) {
    std::string keypair2 = DIDCrypto::GenerateKeyPair();
    EXPECT_FALSE(keypair2.empty());
    // Two generated keypairs should be different
    EXPECT_NE(keypair_, keypair2);
}

TEST_F(DIDCryptoTest, SignData) {
    std::string data = "test_data";
    std::string signature = DIDCrypto::Sign(data, private_key_);
    EXPECT_FALSE(signature.empty());
}

TEST_F(DIDCryptoTest, SignEmptyData) {
    std::string data = "";
    std::string signature = DIDCrypto::Sign(data, private_key_);
    EXPECT_FALSE(signature.empty());
}

TEST_F(DIDCryptoTest, SignWithInvalidKey) {
    std::string data = "test_data";
    // Non-PEM string should fail gracefully
    std::string signature = DIDCrypto::Sign(data, "not-a-pem-key");
    EXPECT_TRUE(signature.empty());
}

TEST_F(DIDCryptoTest, VerifyValidSignature) {
    std::string data = "test_data";
    std::string signature = DIDCrypto::Sign(data, private_key_);
    EXPECT_FALSE(signature.empty());

    bool verified = DIDCrypto::Verify(data, signature, public_key_);
    EXPECT_TRUE(verified);
}

TEST_F(DIDCryptoTest, VerifyWrongData) {
    std::string data = "test_data";
    std::string wrong_data = "wrong_data";

    std::string signature = DIDCrypto::Sign(data, private_key_);
    bool verified = DIDCrypto::Verify(wrong_data, signature, public_key_);
    EXPECT_FALSE(verified);
}

TEST_F(DIDCryptoTest, VerifyWrongKey) {
    std::string data = "test_data";
    std::string signature = DIDCrypto::Sign(data, private_key_);

    // Generate a different keypair
    std::string other_keypair = DIDCrypto::GenerateKeyPair();
    auto pub_pos = other_keypair.find("-----BEGIN PUBLIC KEY-----");
    std::string other_public = other_keypair.substr(pub_pos);

    bool verified = DIDCrypto::Verify(data, signature, other_public);
    EXPECT_FALSE(verified);
}

TEST_F(DIDCryptoTest, VerifyEmptySignature) {
    std::string data = "test_data";
    bool verified = DIDCrypto::Verify(data, "", public_key_);
    EXPECT_FALSE(verified);
}

TEST_F(DIDCryptoTest, HashData) {
    std::string data = "test_data";
    std::string hash = DIDCrypto::Hash(data);
    EXPECT_FALSE(hash.empty());
    // SHA-256 produces 64 hex characters
    EXPECT_EQ(hash.length(), 64u);
}

TEST_F(DIDCryptoTest, HashEmptyData) {
    std::string data = "";
    std::string hash = DIDCrypto::Hash(data);
    EXPECT_FALSE(hash.empty());
}

TEST_F(DIDCryptoTest, HashDeterministic) {
    std::string data = "test_data";
    std::string hash1 = DIDCrypto::Hash(data);
    std::string hash2 = DIDCrypto::Hash(data);
    EXPECT_EQ(hash1, hash2);
}

TEST_F(DIDCryptoTest, HashDifferentData) {
    std::string hash1 = DIDCrypto::Hash("test_data_1");
    std::string hash2 = DIDCrypto::Hash("test_data_2");
    EXPECT_NE(hash1, hash2);
}

TEST_F(DIDCryptoTest, HashLargeData) {
    std::string large_data(10000, 'x');
    std::string hash = DIDCrypto::Hash(large_data);
    EXPECT_FALSE(hash.empty());
    EXPECT_EQ(hash.length(), 64u);
}
