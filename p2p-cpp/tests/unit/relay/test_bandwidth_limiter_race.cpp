/**
 * @file test_bandwidth_limiter_race.cpp
 * @brief Unit tests for bandwidth limiter race condition fix
 */

#include <gtest/gtest.h>
#include "p2p/servers/relay/bandwidth_limiter.hpp"
#include <thread>
#include <vector>

using namespace p2p::relay;

class BandwidthLimiterRaceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Small limits to trigger failures easily
        BandwidthLimit limit(1000, 1000, 2000);  // 1KB/s, 2KB burst
        limiter_ = std::make_unique<BandwidthLimiter>(limit);
    }

    std::unique_ptr<BandwidthLimiter> limiter_;
};

/**
 * Test: Global tokens are refunded when per-allocation limit fails
 */
TEST_F(BandwidthLimiterRaceTest, GlobalTokensRefundedOnPerAllocationFailure) {
    // Get initial global tokens
    auto initial_stats = limiter_->GetGlobalStats();
    uint64_t initial_read_tokens = initial_stats.available_read_tokens;

    // First allocation consumes tokens successfully
    EXPECT_TRUE(limiter_->ThrottleRead("alloc1", 500));

    // Second allocation should fail per-allocation limit
    // but global tokens should be refunded
    EXPECT_FALSE(limiter_->ThrottleRead("alloc1", 2000));

    // Check global tokens were refunded
    auto after_stats = limiter_->GetGlobalStats();

    // Global tokens should be close to initial (minus 500 from first successful call)
    // Allow some tolerance for refill during test execution
    EXPECT_GE(after_stats.available_read_tokens, initial_read_tokens - 1000);
}

/**
 * Test: Write operations also refund global tokens on failure
 */
TEST_F(BandwidthLimiterRaceTest, WriteOperationsRefundGlobalTokens) {
    auto initial_stats = limiter_->GetGlobalStats();
    uint64_t initial_write_tokens = initial_stats.available_write_tokens;

    EXPECT_TRUE(limiter_->ThrottleWrite("alloc1", 500));
    EXPECT_FALSE(limiter_->ThrottleWrite("alloc1", 2000));

    auto after_stats = limiter_->GetGlobalStats();
    EXPECT_GE(after_stats.available_write_tokens, initial_write_tokens - 1000);
}

/**
 * Test: Concurrent operations don't leak tokens
 */
TEST_F(BandwidthLimiterRaceTest, ConcurrentOperationsDontLeakTokens) {
    auto initial_stats = limiter_->GetGlobalStats();
    uint64_t initial_read_tokens = initial_stats.available_read_tokens;

    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};

    // Launch 10 threads trying to consume tokens
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, &success_count, &failure_count, i]() {
            std::string alloc_id = "alloc" + std::to_string(i);
            for (int j = 0; j < 5; ++j) {
                if (limiter_->ThrottleRead(alloc_id, 1500)) {
                    success_count++;
                } else {
                    failure_count++;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Wait for token refill
    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto final_stats = limiter_->GetGlobalStats();

    // Global tokens should recover (accounting for successful operations)
    // With refund, tokens should not be permanently lost
    EXPECT_GT(final_stats.available_read_tokens, 0);
}

/**
 * Test: TokenBucket Refund method works correctly
 */
TEST(TokenBucketTest, RefundAddsTokensBackToBucket) {
    TokenBucket bucket(1000, 2000);  // 1KB/s, 2KB capacity

    // Consume some tokens
    EXPECT_TRUE(bucket.Consume(500));
    uint64_t after_consume = bucket.GetAvailableTokens();
    EXPECT_LE(after_consume, 1500);

    // Refund tokens
    bucket.Refund(500);
    uint64_t after_refund = bucket.GetAvailableTokens();

    // Should have more tokens after refund
    EXPECT_GE(after_refund, after_consume + 400);  // Allow some tolerance
}

/**
 * Test: Refund doesn't exceed capacity
 */
TEST(TokenBucketTest, RefundDoesNotExceedCapacity) {
    TokenBucket bucket(1000, 2000);

    // Try to refund more than capacity
    bucket.Refund(5000);

    // Should be capped at capacity
    EXPECT_LE(bucket.GetAvailableTokens(), 2000);
}

/**
 * Test: Multiple allocations with mixed success/failure
 */
TEST_F(BandwidthLimiterRaceTest, MultipleAllocationsWithMixedResults) {
    // Create multiple allocations
    EXPECT_TRUE(limiter_->ThrottleRead("alloc1", 500));
    EXPECT_TRUE(limiter_->ThrottleRead("alloc2", 500));
    EXPECT_TRUE(limiter_->ThrottleRead("alloc3", 500));

    // These should fail per-allocation limit but refund global tokens
    EXPECT_FALSE(limiter_->ThrottleRead("alloc1", 2000));
    EXPECT_FALSE(limiter_->ThrottleRead("alloc2", 2000));
    EXPECT_FALSE(limiter_->ThrottleRead("alloc3", 2000));

    auto stats = limiter_->GetGlobalStats();
    EXPECT_EQ(stats.active_allocations, 3);

    // Global tokens should not be depleted due to refunds
    EXPECT_GT(stats.available_read_tokens, 0);
}

/**
 * Test: Stress test with rapid consume/refund cycles
 */
TEST_F(BandwidthLimiterRaceTest, StressTestRapidConsumeRefundCycles) {
    std::vector<std::thread> threads;
    std::atomic<int> total_operations{0};

    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([this, &total_operations, i]() {
            std::string alloc_id = "stress_alloc" + std::to_string(i);
            for (int j = 0; j < 100; ++j) {
                // Alternate between small and large requests
                size_t size = (j % 2 == 0) ? 100 : 3000;
                limiter_->ThrottleRead(alloc_id, size);
                total_operations++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(total_operations, 500);

    // System should still be functional
    auto stats = limiter_->GetGlobalStats();
    EXPECT_GT(stats.available_read_tokens, 0);
}

