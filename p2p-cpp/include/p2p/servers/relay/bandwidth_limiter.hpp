/**
 * @file bandwidth_limiter.hpp
 * @brief Bandwidth Control and Rate Limiting
 *
 * Implements token bucket rate limiting and throughput monitoring.
 */

#pragma once

#include <cstdint>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <deque>

namespace p2p {
namespace relay {

/**
 * @brief Token Bucket Rate Limiter
 *
 * Allows bursts up to bucket capacity while maintaining average rate.
 */
class TokenBucket {
public:
    /**
     * @brief Constructor
     * @param rate Tokens per second (bytes per second)
     * @param capacity Maximum bucket size (burst allowance)
     */
    TokenBucket(uint64_t rate, uint64_t capacity);

    /**
     * @brief Try to consume tokens
     * @param tokens Number of tokens to consume
     * @return true if tokens were consumed
     */
    bool Consume(uint64_t tokens);

    /**
     * @brief Refund tokens (e.g., when operation fails after consumption)
     * @param tokens Number of tokens to refund
     */
    void Refund(uint64_t tokens);

    /**
     * @brief Get available tokens
     */
    uint64_t GetAvailableTokens() const;

private:
    void Refill();

    uint64_t rate_;       // tokens per second
    uint64_t capacity_;   // max tokens
    double tokens_;       // current tokens

    mutable std::mutex mutex_;
    std::chrono::steady_clock::time_point last_update_;
};

/**
 * @brief Bandwidth Limit Configuration
 */
struct BandwidthLimit {
    uint64_t read_bps;   // Read bytes per second
    uint64_t write_bps;  // Write bytes per second
    uint64_t burst_bps;  // Burst allowance

    BandwidthLimit()
        : read_bps(3'000'000),   // 3 Mbps
          write_bps(3'000'000),  // 3 Mbps
          burst_bps(6'000'000)   // 6 Mbps burst
    {}

    BandwidthLimit(uint64_t read, uint64_t write, uint64_t burst)
        : read_bps(read), write_bps(write), burst_bps(burst) {}
};

/**
 * @brief Bandwidth Statistics
 */
struct BandwidthStats {
    uint64_t bytes_read = 0;
    uint64_t bytes_written = 0;
    double current_read_rate = 0.0;
    double current_write_rate = 0.0;
    double peak_read_rate = 0.0;
    double peak_write_rate = 0.0;
};

/**
 * @brief Bandwidth Limiter
 *
 * Per-allocation and global bandwidth limiting using token bucket.
 */
class BandwidthLimiter {
public:
    /**
     * @brief Constructor
     * @param default_limit Default bandwidth limit for allocations
     */
    explicit BandwidthLimiter(const BandwidthLimit& default_limit = BandwidthLimit());

    /**
     * @brief Throttle read operation
     * @param allocation_id Allocation ID
     * @param size Number of bytes to read
     * @return true if read allowed
     */
    bool ThrottleRead(const std::string& allocation_id, size_t size);

    /**
     * @brief Throttle write operation
     * @param allocation_id Allocation ID
     * @param size Number of bytes to write
     * @return true if write allowed
     */
    bool ThrottleWrite(const std::string& allocation_id, size_t size);

    /**
     * @brief Set bandwidth limit for allocation
     * @param allocation_id Allocation ID
     * @param limit Bandwidth limit
     */
    void SetLimit(const std::string& allocation_id, const BandwidthLimit& limit);

    /**
     * @brief Remove allocation
     * @param allocation_id Allocation ID
     */
    void RemoveAllocation(const std::string& allocation_id);

    /**
     * @brief Get bandwidth statistics for allocation
     * @param allocation_id Allocation ID
     * @return Statistics
     */
    BandwidthStats GetStats(const std::string& allocation_id) const;

    /**
     * @brief Get global bandwidth statistics
     */
    struct GlobalStats {
        uint64_t available_read_tokens;
        uint64_t available_write_tokens;
        size_t active_allocations;
    };
    GlobalStats GetGlobalStats() const;

private:
    struct AllocationLimiters {
        std::unique_ptr<TokenBucket> read_bucket;
        std::unique_ptr<TokenBucket> write_bucket;
    };

    BandwidthLimit default_limit_;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, AllocationLimiters> limiters_;
    std::unordered_map<std::string, BandwidthStats> stats_;

    // Global limiters
    TokenBucket global_read_bucket_;
    TokenBucket global_write_bucket_;
};

/**
 * @brief Throughput Monitor
 *
 * Tracks bytes transferred and calculates current rates.
 */
class ThroughputMonitor {
public:
    /**
     * @brief Constructor
     * @param window_seconds Time window for rate calculation
     */
    explicit ThroughputMonitor(uint32_t window_seconds = 5);

    /**
     * @brief Record read operation
     * @param bytes_count Number of bytes read
     */
    void RecordRead(size_t bytes_count);

    /**
     * @brief Record write operation
     * @param bytes_count Number of bytes written
     */
    void RecordWrite(size_t bytes_count);

    /**
     * @brief Get current read rate (bytes/second)
     */
    double GetReadRate() const;

    /**
     * @brief Get current write rate (bytes/second)
     */
    double GetWriteRate() const;

    /**
     * @brief Get throughput statistics
     */
    struct Stats {
        double read_rate_bps;
        double write_rate_bps;
        double read_rate_mbps;
        double write_rate_mbps;
    };
    Stats GetStats() const;

private:
    void CleanupOldSamples();

    uint32_t window_seconds_;

    mutable std::mutex mutex_;
    std::deque<std::pair<std::chrono::steady_clock::time_point, size_t>> read_samples_;
    std::deque<std::pair<std::chrono::steady_clock::time_point, size_t>> write_samples_;
};

} // namespace relay
} // namespace p2p
