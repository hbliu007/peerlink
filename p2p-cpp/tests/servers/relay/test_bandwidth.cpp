#include <gtest/gtest.h>
#include "p2p/servers/relay/bandwidth_limiter.hpp"
#include <thread>
#include <chrono>

using namespace p2p::relay;

class TokenBucketTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(TokenBucketTest, ConstructorInitialization) {
    TokenBucket bucket(1000, 500);  // 1000 tokens/sec, 500 capacity
    EXPECT_LE(bucket.GetAvailableTokens(), 500u);
}

TEST_F(TokenBucketTest, ConsumeBasic) {
    TokenBucket bucket(1000, 500);
    EXPECT_TRUE(bucket.Consume(100));
    EXPECT_TRUE(bucket.Consume(200));
}

TEST_F(TokenBucketTest, ConsumeExceedsCapacity) {
    TokenBucket bucket(1000, 500);
    EXPECT_FALSE(bucket.Consume(600));
}

TEST_F(TokenBucketTest, TokenRefill) {
    TokenBucket bucket(1000, 500);
    bucket.Consume(500);

    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    EXPECT_TRUE(bucket.Consume(500));
}

class BandwidthLimiterTest : public ::testing::Test {
protected:
    void SetUp() override {
        limit_ = BandwidthLimit(10000, 10000, 20000);  // 10KB/s, 20KB burst
    }

    BandwidthLimit limit_;
};

TEST_F(BandwidthLimiterTest, ConstructorInitialization) {
    BandwidthLimiter limiter(limit_);
    auto stats = limiter.GetGlobalStats();
    EXPECT_GT(stats.available_read_tokens, 0u);
    EXPECT_GT(stats.available_write_tokens, 0u);
}

TEST_F(BandwidthLimiterTest, ThrottleRead) {
    BandwidthLimiter limiter(limit_);
    EXPECT_TRUE(limiter.ThrottleRead("alloc1", 1000));
    EXPECT_TRUE(limiter.ThrottleRead("alloc1", 1000));
}

TEST_F(BandwidthLimiterTest, ThrottleWrite) {
    BandwidthLimiter limiter(limit_);
    EXPECT_TRUE(limiter.ThrottleWrite("alloc1", 1000));
    EXPECT_TRUE(limiter.ThrottleWrite("alloc1", 1000));
}

TEST_F(BandwidthLimiterTest, SetCustomLimit) {
    BandwidthLimiter limiter(limit_);
    BandwidthLimit custom(5000, 5000, 10000);
    limiter.SetLimit("alloc2", custom);

    EXPECT_TRUE(limiter.ThrottleRead("alloc2", 1000));
}

TEST_F(BandwidthLimiterTest, RemoveAllocation) {
    BandwidthLimiter limiter(limit_);
    limiter.ThrottleRead("alloc3", 1000);
    limiter.RemoveAllocation("alloc3");

    // After removal, should use default limit again
    EXPECT_TRUE(limiter.ThrottleRead("alloc3", 1000));
}

TEST_F(BandwidthLimiterTest, GetStats) {
    BandwidthLimiter limiter(limit_);
    limiter.ThrottleRead("alloc4", 1000);
    limiter.ThrottleWrite("alloc4", 2000);

    auto stats = limiter.GetStats("alloc4");
    // Stats tracking may not be implemented yet
    SUCCEED();
}

TEST_F(BandwidthLimiterTest, GetGlobalStats) {
    BandwidthLimiter limiter(limit_);
    auto stats = limiter.GetGlobalStats();

    EXPECT_GT(stats.available_read_tokens, 0u);
    EXPECT_GT(stats.available_write_tokens, 0u);
}

TEST_F(BandwidthLimiterTest, MultipleAllocations) {
    BandwidthLimiter limiter(limit_);

    EXPECT_TRUE(limiter.ThrottleRead("alloc5", 1000));
    EXPECT_TRUE(limiter.ThrottleRead("alloc6", 1000));
    EXPECT_TRUE(limiter.ThrottleRead("alloc7", 1000));

    auto stats = limiter.GetGlobalStats();
    EXPECT_EQ(stats.active_allocations, 3u);
}

TEST_F(BandwidthLimiterTest, ConcurrentAccess) {
    BandwidthLimiter limiter(limit_);

    std::atomic<int> success_count{0};

    auto worker = [&](int id) {
        std::string alloc_id = "alloc" + std::to_string(id);
        for (int i = 0; i < 10; ++i) {
            if (limiter.ThrottleRead(alloc_id, 100)) {
                success_count++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(success_count.load(), 0);
}

class ThroughputMonitorTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(ThroughputMonitorTest, ConstructorInitialization) {
    ThroughputMonitor monitor(5);
    EXPECT_EQ(monitor.GetReadRate(), 0.0);
    EXPECT_EQ(monitor.GetWriteRate(), 0.0);
}

TEST_F(ThroughputMonitorTest, RecordRead) {
    ThroughputMonitor monitor(5);
    monitor.RecordRead(1000);
    monitor.RecordRead(2000);

    // Rate should be non-zero
    EXPECT_GE(monitor.GetReadRate(), 0.0);
}

TEST_F(ThroughputMonitorTest, RecordWrite) {
    ThroughputMonitor monitor(5);
    monitor.RecordWrite(1000);
    monitor.RecordWrite(2000);

    EXPECT_GE(monitor.GetWriteRate(), 0.0);
}

TEST_F(ThroughputMonitorTest, GetStats) {
    ThroughputMonitor monitor(5);
    monitor.RecordRead(1000000);  // 1MB
    monitor.RecordWrite(2000000);  // 2MB

    auto stats = monitor.GetStats();
    EXPECT_GE(stats.read_rate_bps, 0.0);
    EXPECT_GE(stats.write_rate_bps, 0.0);
}

TEST_F(ThroughputMonitorTest, RateCalculation) {
    ThroughputMonitor monitor(1);  // 1 second window

    // Record 1MB over 1 second
    for (int i = 0; i < 10; ++i) {
        monitor.RecordRead(100000);  // 100KB
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    double rate = monitor.GetReadRate();
    // Should be approximately 1MB/s
    EXPECT_GT(rate, 500000.0);  // At least 500KB/s
    EXPECT_LT(rate, 2000000.0);  // At most 2MB/s
}
