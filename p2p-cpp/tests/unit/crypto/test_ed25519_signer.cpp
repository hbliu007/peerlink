#include <gtest/gtest.h>
#include "p2p/crypto/ed25519_signer.hpp"
#include <vector>
#include <string>

using namespace p2p::crypto;

class Ed25519SignerTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_data_ = {0x01, 0x02, 0x03, 0x04, 0x05};
    }

    std::vector<uint8_t> test_data_;
};

TEST_F(Ed25519SignerTest, GenerateKeyPair) {
    Ed25519PrivateKey private_key = Ed25519Signer::GenerateKeyPair();

    EXPECT_EQ(private_key.GetKeyData().size(), Ed25519PrivateKey::KEY_SIZE);

    std::vector<uint8_t> public_key_data = private_key.GetPublicKey();
    EXPECT_EQ(public_key_data.size(), Ed25519PublicKey::KEY_SIZE);
}

TEST_F(Ed25519SignerTest, SignAndVerify) {
    Ed25519PrivateKey private_key = Ed25519Signer::GenerateKeyPair();
    Ed25519PublicKey public_key = Ed25519Signer::DerivePublicKey(private_key);

    Ed25519Signature signature = Ed25519Signer::Sign(private_key, test_data_);

    EXPECT_EQ(signature.data.size(), Ed25519Signature::SIGNATURE_SIZE);

    bool valid = Ed25519Signer::Verify(public_key, test_data_, signature);
    EXPECT_TRUE(valid);
}

TEST_F(Ed25519SignerTest, VerifyInvalidSignature) {
    Ed25519PrivateKey private_key = Ed25519Signer::GenerateKeyPair();
    Ed25519PublicKey public_key = Ed25519Signer::DerivePublicKey(private_key);

    Ed25519Signature signature = Ed25519Signer::Sign(private_key, test_data_);

    signature.data[0] ^= 0xFF;

    bool valid = Ed25519Signer::Verify(public_key, test_data_, signature);
    EXPECT_FALSE(valid);
}

TEST_F(Ed25519SignerTest, VerifyWithWrongPublicKey) {
    Ed25519PrivateKey private_key1 = Ed25519Signer::GenerateKeyPair();
    Ed25519PrivateKey private_key2 = Ed25519Signer::GenerateKeyPair();
    Ed25519PublicKey public_key2 = Ed25519Signer::DerivePublicKey(private_key2);

    Ed25519Signature signature = Ed25519Signer::Sign(private_key1, test_data_);

    bool valid = Ed25519Signer::Verify(public_key2, test_data_, signature);
    EXPECT_FALSE(valid);
}

TEST_F(Ed25519SignerTest, VerifyWithModifiedData) {
    Ed25519PrivateKey private_key = Ed25519Signer::GenerateKeyPair();
    Ed25519PublicKey public_key = Ed25519Signer::DerivePublicKey(private_key);

    Ed25519Signature signature = Ed25519Signer::Sign(private_key, test_data_);

    std::vector<uint8_t> modified_data = test_data_;
    modified_data[0] ^= 0xFF;

    bool valid = Ed25519Signer::Verify(public_key, modified_data, signature);
    EXPECT_FALSE(valid);
}

TEST_F(Ed25519SignerTest, DerivePublicKey) {
    Ed25519PrivateKey private_key = Ed25519Signer::GenerateKeyPair();

    Ed25519PublicKey public_key1 = Ed25519Signer::DerivePublicKey(private_key);
    Ed25519PublicKey public_key2 = Ed25519Signer::DerivePublicKey(private_key);

    EXPECT_EQ(public_key1.GetKeyData(), public_key2.GetKeyData());
}

TEST_F(Ed25519SignerTest, InvalidPrivateKeySize) {
    std::vector<uint8_t> invalid_key(16);

    EXPECT_THROW({
        Ed25519PrivateKey key(invalid_key);
    }, std::invalid_argument);
}

TEST_F(Ed25519SignerTest, InvalidPublicKeySize) {
    std::vector<uint8_t> invalid_key(16);

    EXPECT_THROW({
        Ed25519PublicKey key(invalid_key);
    }, std::invalid_argument);
}

TEST_F(Ed25519SignerTest, SignEmptyData) {
    Ed25519PrivateKey private_key = Ed25519Signer::GenerateKeyPair();
    Ed25519PublicKey public_key = Ed25519Signer::DerivePublicKey(private_key);

    std::vector<uint8_t> empty_data;
    Ed25519Signature signature = Ed25519Signer::Sign(private_key, empty_data);

    bool valid = Ed25519Signer::Verify(public_key, empty_data, signature);
    EXPECT_TRUE(valid);
}

TEST_F(Ed25519SignerTest, SignLargeData) {
    Ed25519PrivateKey private_key = Ed25519Signer::GenerateKeyPair();
    Ed25519PublicKey public_key = Ed25519Signer::DerivePublicKey(private_key);

    std::vector<uint8_t> large_data(1024 * 1024, 0xAB);

    Ed25519Signature signature = Ed25519Signer::Sign(private_key, large_data);

    bool valid = Ed25519Signer::Verify(public_key, large_data, signature);
    EXPECT_TRUE(valid);
}
