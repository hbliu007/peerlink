// Unit tests for rate limiter

#include <gtest/gtest.h>
#include "p2p/servers/relay/rate_limiter.hpp"
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>

using namespace p2p::relay;

class RateLimiterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 5 requests/sec, 10 burst, 3 violations = ban for 5 seconds
        RateLimitConfig config(5, 10, 3, 5);
        limiter_ = std::make_unique<RateLimiter>(config);
    }

    std::unique_ptr<RateLimiter> limiter_;
};

TEST_F(RateLimiterTest, AllowRequestsWithinLimit) {
    std::string client = "192.168.1.100:12345";

    // Should allow burst of 10 requests
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(limiter_->AllowRequest(client));
    }

    // 11th request should be blocked
    EXPECT_FALSE(limiter_->AllowRequest(client));
}

TEST_F(RateLimiterTest, TokenRefillOverTime) {
    // Use a dedicated limiter: ban_duration=0 disables ban so we can test refill
    RateLimitConfig config(5, 10, 100, 0);  // 5 rps, 10 burst, no ban
    auto refill_limiter = std::make_unique<RateLimiter>(config);
    std::string client = "192.168.1.100:12345";

    // Consume all 10 tokens
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(refill_limiter->AllowRequest(client));
    }

    // Should be blocked (0 tokens remaining)
    EXPECT_FALSE(refill_limiter->AllowRequest(client));

    // Wait 1.2 seconds: rate=5 tokens/sec → ~6 tokens refilled
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));

    // Should allow at least 4 requests (tolerance for timing jitter)
    int allowed = 0;
    for (int i = 0; i < 6; ++i) {
        if (refill_limiter->AllowRequest(client)) allowed++;
    }
    EXPECT_GE(allowed, 4);
}

TEST_F(RateLimiterTest, BanAfterThresholdViolations) {
    std::string client = "192.168.1.100:12345";

    // Consume all tokens
    for (int i = 0; i < 10; ++i) {
        limiter_->AllowRequest(client);
    }

    // Trigger 3 violations (ban threshold)
    for (int i = 0; i < 3; ++i) {
        EXPECT_FALSE(limiter_->AllowRequest(client));
    }

    // Should be banned now
    EXPECT_TRUE(limiter_->IsBanned(client));

    // Wait for token refill
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Still banned (ban duration is 5 seconds)
    EXPECT_FALSE(limiter_->AllowRequest(client));
}

TEST_F(RateLimiterTest, BanExpiresAfterDuration) {
    std::string client = "192.168.1.100:12345";

    // Trigger ban
    for (int i = 0; i < 10; ++i) {
        limiter_->AllowRequest(client);
    }
    for (int i = 0; i < 3; ++i) {
        limiter_->AllowRequest(client);
    }

    EXPECT_TRUE(limiter_->IsBanned(client));

    // Wait for ban to expire (5 seconds)
    std::this_thread::sleep_for(std::chrono::seconds(6));

    // Should not be banned anymore
    EXPECT_FALSE(limiter_->IsBanned(client));

    // Should allow requests again
    EXPECT_TRUE(limiter_->AllowRequest(client));
}

TEST_F(RateLimiterTest, MultipleClientsIndependent) {
    std::string client1 = "192.168.1.100:12345";
    std::string client2 = "192.168.1.101:12346";

    // Client1 consumes all tokens
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(limiter_->AllowRequest(client1));
    }
    EXPECT_FALSE(limiter_->AllowRequest(client1));

    // Client2 should still have tokens
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(limiter_->AllowRequest(client2));
    }
}

TEST_F(RateLimiterTest, ManualBan) {
    std::string client = "192.168.1.100:12345";

    // Manually ban client
    limiter_->BanClient(client);

    EXPECT_TRUE(limiter_->IsBanned(client));
    EXPECT_FALSE(limiter_->AllowRequest(client));
}

TEST_F(RateLimiterTest, ManualUnban) {
    std::string client = "192.168.1.100:12345";

    // Ban and then unban
    limiter_->BanClient(client);
    EXPECT_TRUE(limiter_->IsBanned(client));

    limiter_->UnbanClient(client);
    EXPECT_FALSE(limiter_->IsBanned(client));

    // Should allow requests
    EXPECT_TRUE(limiter_->AllowRequest(client));
}

TEST_F(RateLimiterTest, CleanupExpiredClients) {
    // Create multiple clients
    for (int i = 0; i < 10; ++i) {
        std::string client = "192.168.1." + std::to_string(i) + ":12345";
        limiter_->AllowRequest(client);
    }

    auto stats = limiter_->GetStats();
    EXPECT_EQ(stats.total_clients, 10);

    // Cleanup (should remove clients with no violations and not banned)
    size_t cleaned = limiter_->CleanupExpired();
    EXPECT_GT(cleaned, 0u);

    stats = limiter_->GetStats();
    EXPECT_LT(stats.total_clients, 10u);
}

TEST_F(RateLimiterTest, StatisticsTracking) {
    std::string client = "192.168.1.100:12345";

    // Make some requests
    for (int i = 0; i < 15; ++i) {
        limiter_->AllowRequest(client);
    }

    auto stats = limiter_->GetStats();
    EXPECT_EQ(stats.total_requests, 15u);
    EXPECT_EQ(stats.blocked_requests, 5u);  // 10 allowed, 5 blocked
    EXPECT_GT(stats.block_rate, 0.0);
    EXPECT_LT(stats.block_rate, 1.0);
}

TEST_F(RateLimiterTest, ConcurrentRequestsSameClient) {
    std::string client = "192.168.1.100:12345";
    std::atomic<int> allowed{0};
    std::atomic<int> blocked{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < 5; ++t) {
        threads.emplace_back([this, &client, &allowed, &blocked]() {
            for (int i = 0; i < 10; ++i) {
                if (limiter_->AllowRequest(client)) {
                    allowed++;
                } else {
                    blocked++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Total 50 requests, should allow ~10 (burst size)
    EXPECT_LE(allowed.load(), 15);  // Allow some tolerance
    EXPECT_GE(blocked.load(), 35);
}

TEST_F(RateLimiterTest, ConcurrentRequestsDifferentClients) {
    std::atomic<int> total_allowed{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < 10; ++t) {
        threads.emplace_back([this, &total_allowed, t]() {
            std::string client = "192.168.1." + std::to_string(t) + ":12345";
            for (int i = 0; i < 10; ++i) {
                if (limiter_->AllowRequest(client)) {
                    total_allowed++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Each client should get ~10 requests (burst size)
    EXPECT_GE(total_allowed.load(), 90);  // Most should be allowed
}

TEST_F(RateLimiterTest, RemoveClient) {
    std::string client = "192.168.1.100:12345";

    limiter_->AllowRequest(client);

    auto stats = limiter_->GetStats();
    EXPECT_EQ(stats.total_clients, 1u);

    limiter_->RemoveClient(client);

    stats = limiter_->GetStats();
    EXPECT_EQ(stats.total_clients, 0u);
}

TEST_F(RateLimiterTest, HighLoadStressTest) {
    std::atomic<int> total_requests{0};
    std::atomic<int> total_allowed{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < 20; ++t) {
        threads.emplace_back([this, &total_requests, &total_allowed, t]() {
            std::string client = "192.168.1." + std::to_string(t % 5) + ":12345";
            for (int i = 0; i < 100; ++i) {
                total_requests++;
                if (limiter_->AllowRequest(client)) {
                    total_allowed++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(total_requests.load(), 2000);

    auto stats = limiter_->GetStats();
    EXPECT_EQ(stats.total_requests, 2000u);
    EXPECT_GT(stats.blocked_requests, 0u);
    EXPECT_LE(stats.total_clients, 5u);
}

// --- ClientRateLimiter Direct Tests ---

class ClientRateLimiterTest : public ::testing::Test {};

TEST_F(ClientRateLimiterTest, BasicAllowAndDeny) {
    ClientRateLimiter limiter(2, 3);  // 2 tokens/sec, 3 capacity

    // Should allow up to capacity
    EXPECT_TRUE(limiter.AllowRequest());
    EXPECT_TRUE(limiter.AllowRequest());
    EXPECT_TRUE(limiter.AllowRequest());

    // Should deny when exhausted
    EXPECT_FALSE(limiter.AllowRequest());
}

TEST_F(ClientRateLimiterTest, ViolationTracking) {
    ClientRateLimiter limiter(10, 10);

    EXPECT_EQ(limiter.GetViolationCount(), 0u);

    limiter.RecordViolation();
    EXPECT_EQ(limiter.GetViolationCount(), 1u);

    limiter.RecordViolation();
    limiter.RecordViolation();
    EXPECT_EQ(limiter.GetViolationCount(), 3u);
}

TEST_F(ClientRateLimiterTest, ResetViolations) {
    ClientRateLimiter limiter(10, 10);

    limiter.RecordViolation();
    limiter.RecordViolation();
    EXPECT_EQ(limiter.GetViolationCount(), 2u);

    limiter.ResetViolations();
    EXPECT_EQ(limiter.GetViolationCount(), 0u);
}

TEST_F(ClientRateLimiterTest, IsBannedWithinDuration) {
    ClientRateLimiter limiter(10, 10);

    // Not banned initially
    EXPECT_FALSE(limiter.IsBanned(5));

    // Record a violation
    limiter.RecordViolation();

    // Should be banned within duration
    EXPECT_TRUE(limiter.IsBanned(5));
}

TEST_F(ClientRateLimiterTest, IsBannedNeverViolated) {
    ClientRateLimiter limiter(10, 10);

    // Never violated, should never be banned
    EXPECT_FALSE(limiter.IsBanned(0));
    EXPECT_FALSE(limiter.IsBanned(100));
}

// --- RateLimitConfig Tests ---

TEST(RateLimitConfigTest, DefaultValues) {
    RateLimitConfig config;
    EXPECT_EQ(config.requests_per_second, 10u);
    EXPECT_EQ(config.burst_size, 20u);
    EXPECT_EQ(config.ban_threshold, 5u);
    EXPECT_EQ(config.ban_duration_seconds, 60u);
}

TEST(RateLimitConfigTest, CustomValues) {
    RateLimitConfig config(100, 200, 10, 120);
    EXPECT_EQ(config.requests_per_second, 100u);
    EXPECT_EQ(config.burst_size, 200u);
    EXPECT_EQ(config.ban_threshold, 10u);
    EXPECT_EQ(config.ban_duration_seconds, 120u);
}

TEST_F(RateLimiterTest, IsBannedUnknownClient) {
    EXPECT_FALSE(limiter_->IsBanned("unknown_client:0"));
}

TEST_F(RateLimiterTest, UnbanNonexistentClient) {
    // Should not crash
    limiter_->UnbanClient("nonexistent:0");
}

TEST_F(RateLimiterTest, RemoveNonexistentClient) {
    // Should not crash
    limiter_->RemoveClient("nonexistent:0");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
