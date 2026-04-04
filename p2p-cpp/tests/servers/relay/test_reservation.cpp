// Reservation Manager Tests

#include <gtest/gtest.h>
#include "p2p/servers/relay/reservation_manager.hpp"
#include <thread>
#include <chrono>

using namespace p2p::relay;

class ReservationManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager = std::make_unique<ReservationManager>(10, 2);  // Max 10, 2 seconds lifetime
    }

    std::unique_ptr<ReservationManager> manager;
};

TEST_F(ReservationManagerTest, CreateReservation) {
    auto token = manager->CreateReservation("client1");

    ASSERT_TRUE(token.has_value());
    EXPECT_FALSE(token->token_id.empty());
    EXPECT_EQ(token->client_id, "client1");
    EXPECT_FALSE(token->used);
    EXPECT_FALSE(token->IsExpired());
}

TEST_F(ReservationManagerTest, ValidateAndConsumeToken) {
    auto token = manager->CreateReservation("client1");
    ASSERT_TRUE(token.has_value());

    // First validation should succeed
    EXPECT_TRUE(manager->ValidateAndConsumeToken(token->token_id, "client1"));

    // Second validation should fail (token already used)
    EXPECT_FALSE(manager->ValidateAndConsumeToken(token->token_id, "client1"));
}

TEST_F(ReservationManagerTest, ValidateWrongClient) {
    auto token = manager->CreateReservation("client1");
    ASSERT_TRUE(token.has_value());

    // Validation with wrong client ID should fail
    EXPECT_FALSE(manager->ValidateAndConsumeToken(token->token_id, "client2"));
}

TEST_F(ReservationManagerTest, ValidateInvalidToken) {
    // Validation with non-existent token should fail
    EXPECT_FALSE(manager->ValidateAndConsumeToken("invalid-token", "client1"));
}

TEST_F(ReservationManagerTest, TokenExpiration) {
    auto token = manager->CreateReservation("client1", 1);  // 1 second lifetime
    ASSERT_TRUE(token.has_value());

    EXPECT_FALSE(token->IsExpired());

    // Wait for expiration (add extra time to ensure expiration)
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));

    // Create a new token object to check expiration
    ReservationToken expired_token = *token;
    EXPECT_TRUE(expired_token.IsExpired());

    // Validation should fail for expired token
    EXPECT_FALSE(manager->ValidateAndConsumeToken(token->token_id, "client1"));
}

TEST_F(ReservationManagerTest, MaxReservations) {
    // Create 10 reservations (max limit)
    for (int i = 0; i < 10; ++i) {
        auto token = manager->CreateReservation("client" + std::to_string(i));
        EXPECT_TRUE(token.has_value());
    }

    // 11th reservation should fail
    auto token = manager->CreateReservation("client11");
    EXPECT_FALSE(token.has_value());
}

TEST_F(ReservationManagerTest, CleanupExpired) {
    // Create reservations with 1 second lifetime
    auto token1 = manager->CreateReservation("client1", 1);
    auto token2 = manager->CreateReservation("client2", 1);
    auto token3 = manager->CreateReservation("client3", 10);  // Long lifetime

    ASSERT_TRUE(token1.has_value());
    ASSERT_TRUE(token2.has_value());
    ASSERT_TRUE(token3.has_value());

    // Wait for first two to expire
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    size_t cleaned = manager->CleanupExpired();
    EXPECT_EQ(cleaned, 2);

    // Third reservation should still be valid
    EXPECT_TRUE(manager->ValidateAndConsumeToken(token3->token_id, "client3"));
}

TEST_F(ReservationManagerTest, GetStats) {
    manager->CreateReservation("client1");
    manager->CreateReservation("client2");

    auto stats = manager->GetStats();
    EXPECT_EQ(stats.total_reservations, 2);
    EXPECT_EQ(stats.active_reservations, 2);
    EXPECT_EQ(stats.used_reservations, 0);
    EXPECT_EQ(stats.max_reservations, 10);
}

TEST_F(ReservationManagerTest, HasValidReservation) {
    auto token = manager->CreateReservation("client1");
    ASSERT_TRUE(token.has_value());

    EXPECT_TRUE(manager->HasValidReservation("client1"));
    EXPECT_FALSE(manager->HasValidReservation("client2"));

    // After consuming token
    manager->ValidateAndConsumeToken(token->token_id, "client1");
    EXPECT_FALSE(manager->HasValidReservation("client1"));
}
