#include <gtest/gtest.h>
#include "servers/did/did_auth.hpp"
#include <thread>
#include <chrono>
#include <vector>
#include <mutex>

using namespace p2p::did;

class DIDAuthTest : public ::testing::Test {
protected:
    std::unique_ptr<DidAuth> auth;

    void SetUp() override {
        auth = std::make_unique<DidAuth>("test_secret_key");
    }

    void TearDown() override {
        auth.reset();
    }
};

TEST_F(DIDAuthTest, GenerateToken) {
    std::string token = auth->GenerateToken("did:example:123");
    EXPECT_FALSE(token.empty());
}

TEST_F(DIDAuthTest, GenerateTokenEmptyDID) {
    // Empty DID should be rejected — no token generated
    std::string token = auth->GenerateToken("");
    EXPECT_TRUE(token.empty());
}

TEST_F(DIDAuthTest, GenerateMultipleTokens) {
    std::string token1 = auth->GenerateToken("did:example:123");
    std::string token2 = auth->GenerateToken("did:example:456");

    EXPECT_FALSE(token1.empty());
    EXPECT_FALSE(token2.empty());
}

TEST_F(DIDAuthTest, ValidateToken) {
    std::string token = auth->GenerateToken("did:example:123");
    bool valid = auth->ValidateToken(token);

    EXPECT_TRUE(valid);
}

TEST_F(DIDAuthTest, ValidateEmptyToken) {
    bool valid = auth->ValidateToken("");
    EXPECT_FALSE(valid);
}

TEST_F(DIDAuthTest, ValidateInvalidToken) {
    bool valid = auth->ValidateToken("invalid_token");
    EXPECT_FALSE(valid);
}

TEST_F(DIDAuthTest, ExtractDID) {
    std::string token = auth->GenerateToken("did:example:123");
    std::string did = auth->ExtractDid(token);

    EXPECT_FALSE(did.empty());
    EXPECT_EQ(did, "did:example:123");
}

TEST_F(DIDAuthTest, ExtractDIDEmptyToken) {
    std::string did = auth->ExtractDid("");
    EXPECT_TRUE(did.empty());
}

TEST_F(DIDAuthTest, TokenLifecycle) {
    std::string did = "did:example:123";
    std::string token = auth->GenerateToken(did);

    EXPECT_FALSE(token.empty());
    EXPECT_TRUE(auth->ValidateToken(token));

    std::string extracted_did = auth->ExtractDid(token);
    EXPECT_FALSE(extracted_did.empty());
}

TEST_F(DIDAuthTest, LongDID) {
    std::string long_did = "did:example:" + std::string(1000, 'x');
    std::string token = auth->GenerateToken(long_did);

    EXPECT_FALSE(token.empty());
}

TEST_F(DIDAuthTest, SpecialCharactersDID) {
    std::string special_did = "did:example:abc-123_456.789";
    std::string token = auth->GenerateToken(special_did);

    EXPECT_FALSE(token.empty());
}
