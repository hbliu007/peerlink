/**
 * @file rate_limiter.cpp
 * @brief Rate Limiter Implementation
 */

#include "p2p/servers/relay/rate_limiter.hpp"

namespace p2p {
namespace relay {

// ClientRateLimiter Implementation

ClientRateLimiter::ClientRateLimiter(uint32_t rate, uint32_t capacity)
    : rate_(rate),
      capacity_(capacity),
      tokens_(static_cast<double>(capacity)),
      last_update_(std::chrono::steady_clock::now()),
      last_violation_(std::chrono::steady_clock::now()) {}

void ClientRateLimiter::Refill() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double>(now - last_update_).count();

    tokens_ += elapsed * rate_;
    if (tokens_ > capacity_) {
        tokens_ = capacity_;
    }

    last_update_ = now;
}

bool ClientRateLimiter::AllowRequest() {
    std::lock_guard<std::mutex> lock(mutex_);

    Refill();

    if (tokens_ >= 1.0) {
        tokens_ -= 1.0;
        return true;
    }

    return false;
}

void ClientRateLimiter::RecordViolation() {
    std::lock_guard<std::mutex> lock(mutex_);
    violations_++;
    last_violation_ = std::chrono::steady_clock::now();
}

bool ClientRateLimiter::IsBanned(uint32_t ban_duration_seconds) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (violations_ == 0) {
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - last_violation_).count();

    return elapsed < ban_duration_seconds;
}

// RateLimiter Implementation

RateLimiter::RateLimiter(const RateLimitConfig& config)
    : config_(config) {}

bool RateLimiter::AllowRequest(const std::string& client_addr) {
    std::lock_guard<std::mutex> lock(mutex_);

    total_requests_++;

    auto& limiter = limiters_[client_addr];
    if (!limiter) {
        limiter = std::make_unique<ClientRateLimiter>(
            config_.requests_per_second,
            config_.burst_size);
    }

    if (limiter->IsBanned(config_.ban_duration_seconds)) {
        blocked_requests_++;
        return false;
    }

    if (!limiter->AllowRequest()) {
        limiter->RecordViolation();

        if (limiter->GetViolationCount() >= config_.ban_threshold) {
            blocked_requests_++;
            return false;
        }

        blocked_requests_++;
        return false;
    }

    return true;
}

bool RateLimiter::IsBanned(const std::string& client_addr) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = limiters_.find(client_addr);
    if (it == limiters_.end()) {
        return false;
    }

    return it->second->IsBanned(config_.ban_duration_seconds);
}

void RateLimiter::BanClient(const std::string& client_addr, uint32_t duration_seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    (void)duration_seconds;

    auto& limiter = limiters_[client_addr];
    if (!limiter) {
        limiter = std::make_unique<ClientRateLimiter>(
            config_.requests_per_second,
            config_.burst_size);
    }

    for (uint32_t i = 0; i < config_.ban_threshold; ++i) {
        limiter->RecordViolation();
    }
}

void RateLimiter::UnbanClient(const std::string& client_addr) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = limiters_.find(client_addr);
    if (it != limiters_.end()) {
        it->second->ResetViolations();
    }
}

void RateLimiter::RemoveClient(const std::string& client_addr) {
    std::lock_guard<std::mutex> lock(mutex_);
    limiters_.erase(client_addr);
}

size_t RateLimiter::CleanupExpired() {
    std::vector<std::string> expired_clients;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        expired_clients.reserve(limiters_.size() / 10);

        for (const auto& [addr, limiter] : limiters_) {
            if (!limiter->IsBanned(config_.ban_duration_seconds) &&
                limiter->GetViolationCount() == 0) {
                expired_clients.push_back(addr);
            }
        }
    }

    for (const auto& addr : expired_clients) {
        RemoveClient(addr);
    }

    return expired_clients.size();
}

RateLimiter::Stats RateLimiter::GetStats() const {
    std::lock_guard<std::mutex> lock(mutex_);

    Stats stats;
    stats.total_clients = limiters_.size();
    stats.banned_clients = 0;
    stats.total_requests = total_requests_;
    stats.blocked_requests = blocked_requests_;

    for (const auto& [addr, limiter] : limiters_) {
        if (limiter->IsBanned(config_.ban_duration_seconds)) {
            stats.banned_clients++;
        }
    }

    stats.block_rate = stats.total_requests > 0
        ? (stats.blocked_requests / static_cast<double>(stats.total_requests))
        : 0.0;

    return stats;
}

} // namespace relay
} // namespace p2p
