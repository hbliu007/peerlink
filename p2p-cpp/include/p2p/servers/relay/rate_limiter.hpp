/**
 * @file rate_limiter.hpp
 * @brief Rate Limiter for Control Messages
 *
 * Implements token bucket rate limiting for TURN control messages.
 */

#pragma once

#include <cstdint>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace p2p {
namespace relay {

/**
 * @brief Rate Limit Configuration
 */
struct RateLimitConfig {
    uint32_t requests_per_second;  // Max requests per second
    uint32_t burst_size;            // Max burst size
    uint32_t ban_threshold;         // Violations before ban
    uint32_t ban_duration_seconds;  // Ban duration

    RateLimitConfig()
        : requests_per_second(10),
          burst_size(20),
          ban_threshold(5),
          ban_duration_seconds(60) {}

    RateLimitConfig(uint32_t rps, uint32_t burst, uint32_t threshold, uint32_t ban_duration)
        : requests_per_second(rps),
          burst_size(burst),
          ban_threshold(threshold),
          ban_duration_seconds(ban_duration) {}
};

/**
 * @brief Per-Client Rate Limiter State
 */
class ClientRateLimiter {
public:
    ClientRateLimiter(uint32_t rate, uint32_t capacity);

    /**
     * @brief Check if request is allowed
     * @return true if allowed, false if rate limited
     */
    bool AllowRequest();

    /**
     * @brief Record a violation
     */
    void RecordViolation();

    /**
     * @brief Check if client is banned
     */
    bool IsBanned(uint32_t ban_duration_seconds) const;

    /**
     * @brief Get violation count
     */
    uint32_t GetViolationCount() const { return violations_; }

    /**
     * @brief Reset violations
     */
    void ResetViolations() { violations_ = 0; }

private:
    void Refill();

    uint32_t rate_;       // tokens per second
    uint32_t capacity_;   // max tokens
    double tokens_;       // current tokens

    std::chrono::steady_clock::time_point last_update_;
    std::chrono::steady_clock::time_point last_violation_;

    uint32_t violations_ = 0;
    mutable std::mutex mutex_;
};

/**
 * @brief Rate Limiter for Control Messages
 *
 * Per-client rate limiting with automatic ban for abusive clients.
 */
class RateLimiter {
public:
    /**
     * @brief Constructor
     * @param config Rate limit configuration
     */
    explicit RateLimiter(const RateLimitConfig& config = RateLimitConfig());

    /**
     * @brief Check if request is allowed
     * @param client_addr Client address (IP:port)
     * @return true if allowed, false if rate limited
     */
    bool AllowRequest(const std::string& client_addr);

    /**
     * @brief Check if client is banned
     * @param client_addr Client address
     */
    bool IsBanned(const std::string& client_addr) const;

    /**
     * @brief Manually ban a client
     * @param client_addr Client address
     * @param duration_seconds Ban duration (0 = use default)
     */
    void BanClient(const std::string& client_addr, uint32_t duration_seconds = 0);

    /**
     * @brief Unban a client
     * @param client_addr Client address
     */
    void UnbanClient(const std::string& client_addr);

    /**
     * @brief Remove client state (cleanup)
     * @param client_addr Client address
     */
    void RemoveClient(const std::string& client_addr);

    /**
     * @brief Cleanup expired bans and inactive clients
     * @return Number of clients removed
     */
    size_t CleanupExpired();

    /**
     * @brief Get statistics
     */
    struct Stats {
        size_t total_clients;
        size_t banned_clients;
        size_t total_requests;
        size_t blocked_requests;
        double block_rate;
    };
    Stats GetStats() const;

private:
    RateLimitConfig config_;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<ClientRateLimiter>> limiters_;

    // Statistics
    uint64_t total_requests_ = 0;
    uint64_t blocked_requests_ = 0;
};

} // namespace relay
} // namespace p2p

