/**
 * @file bandwidth_limiter.cpp
 * @brief Bandwidth Limiter Implementation
 */

#include "p2p/servers/relay/bandwidth_limiter.hpp"

namespace p2p {
namespace relay {

// TokenBucket Implementation

TokenBucket::TokenBucket(uint64_t rate, uint64_t capacity)
    : rate_(rate),
      capacity_(capacity),
      tokens_(static_cast<double>(capacity)),
      last_update_(std::chrono::steady_clock::now()) {}

void TokenBucket::Refill() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double>(now - last_update_).count();

    tokens_ += elapsed * rate_;
    if (tokens_ > capacity_) {
        tokens_ = capacity_;
    }

    last_update_ = now;
}

bool TokenBucket::Consume(uint64_t tokens) {
    std::lock_guard<std::mutex> lock(mutex_);

    Refill();

    if (tokens_ >= tokens) {
        tokens_ -= tokens;
        return true;
    }

    return false;
}

void TokenBucket::Refund(uint64_t tokens) {
    std::lock_guard<std::mutex> lock(mutex_);

    tokens_ += tokens;
    if (tokens_ > capacity_) {
        tokens_ = capacity_;
    }
}

uint64_t TokenBucket::GetAvailableTokens() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<uint64_t>(tokens_);
}

// BandwidthLimiter Implementation

BandwidthLimiter::BandwidthLimiter(const BandwidthLimit& default_limit)
    : default_limit_(default_limit),
      global_read_bucket_(100'000'000, 200'000'000),
      global_write_bucket_(100'000'000, 200'000'000) {}

bool BandwidthLimiter::ThrottleRead(const std::string& allocation_id, size_t size) {
    if (!global_read_bucket_.Consume(size)) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    auto& limiters = limiters_[allocation_id];
    if (!limiters.read_bucket) {
        limiters.read_bucket = std::make_unique<TokenBucket>(
            default_limit_.read_bps,
            default_limit_.burst_bps);
    }

    if (limiters.read_bucket->Consume(size)) {
        stats_[allocation_id].bytes_read += size;
        return true;
    }

    global_read_bucket_.Refund(size);
    return false;
}

bool BandwidthLimiter::ThrottleWrite(const std::string& allocation_id, size_t size) {
    if (!global_write_bucket_.Consume(size)) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    auto& limiters = limiters_[allocation_id];
    if (!limiters.write_bucket) {
        limiters.write_bucket = std::make_unique<TokenBucket>(
            default_limit_.write_bps,
            default_limit_.burst_bps);
    }

    if (limiters.write_bucket->Consume(size)) {
        stats_[allocation_id].bytes_written += size;
        return true;
    }

    global_write_bucket_.Refund(size);
    return false;
}

void BandwidthLimiter::SetLimit(
    const std::string& allocation_id,
    const BandwidthLimit& limit) {

    std::lock_guard<std::mutex> lock(mutex_);

    auto& limiters = limiters_[allocation_id];
    limiters.read_bucket = std::make_unique<TokenBucket>(limit.read_bps, limit.burst_bps);
    limiters.write_bucket = std::make_unique<TokenBucket>(limit.write_bps, limit.burst_bps);
}

void BandwidthLimiter::RemoveAllocation(const std::string& allocation_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    limiters_.erase(allocation_id);
    stats_.erase(allocation_id);
}

BandwidthStats BandwidthLimiter::GetStats(const std::string& allocation_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = stats_.find(allocation_id);
    return it != stats_.end() ? it->second : BandwidthStats();
}

BandwidthLimiter::GlobalStats BandwidthLimiter::GetGlobalStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    GlobalStats stats;
    stats.available_read_tokens = global_read_bucket_.GetAvailableTokens();
    stats.available_write_tokens = global_write_bucket_.GetAvailableTokens();
    stats.active_allocations = limiters_.size();
    return stats;
}

// ThroughputMonitor Implementation

ThroughputMonitor::ThroughputMonitor(uint32_t window_seconds)
    : window_seconds_(window_seconds) {}

void ThroughputMonitor::RecordRead(size_t bytes_count) {
    std::lock_guard<std::mutex> lock(mutex_);
    read_samples_.emplace_back(std::chrono::steady_clock::now(), bytes_count);
    CleanupOldSamples();
}

void ThroughputMonitor::RecordWrite(size_t bytes_count) {
    std::lock_guard<std::mutex> lock(mutex_);
    write_samples_.emplace_back(std::chrono::steady_clock::now(), bytes_count);
    CleanupOldSamples();
}

void ThroughputMonitor::CleanupOldSamples() {
    auto cutoff = std::chrono::steady_clock::now() - std::chrono::seconds(window_seconds_);

    while (!read_samples_.empty() && read_samples_.front().first < cutoff) {
        read_samples_.pop_front();
    }

    while (!write_samples_.empty() && write_samples_.front().first < cutoff) {
        write_samples_.pop_front();
    }
}

double ThroughputMonitor::GetReadRate() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (read_samples_.empty()) {
        return 0.0;
    }

    uint64_t total_bytes = 0;
    for (const auto& [time, bytes] : read_samples_) {
        total_bytes += bytes;
    }

    return static_cast<double>(total_bytes) / window_seconds_;
}

double ThroughputMonitor::GetWriteRate() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (write_samples_.empty()) {
        return 0.0;
    }

    uint64_t total_bytes = 0;
    for (const auto& [time, bytes] : write_samples_) {
        total_bytes += bytes;
    }

    return static_cast<double>(total_bytes) / window_seconds_;
}

ThroughputMonitor::Stats ThroughputMonitor::GetStats() const {
    Stats stats;
    stats.read_rate_bps = GetReadRate();
    stats.write_rate_bps = GetWriteRate();
    stats.read_rate_mbps = stats.read_rate_bps / 1'000'000.0;
    stats.write_rate_mbps = stats.write_rate_bps / 1'000'000.0;
    return stats;
}

} // namespace relay
} // namespace p2p
