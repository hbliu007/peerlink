/**
 * @file port_pool.cpp
 * @brief Port Pool Implementation
 */

#include "p2p/servers/relay/port_pool.hpp"

#include <algorithm>
#include <stdexcept>

namespace p2p {
namespace relay {

PortPool::PortPool(uint16_t min_port, uint16_t max_port)
    : min_port_(min_port),
      max_port_(max_port),
      total_ports_(max_port - min_port + 1),
      rng_(std::random_device{}()) {

    if (min_port >= max_port) {
        throw std::invalid_argument("min_port must be less than max_port");
    }

    if (total_ports_ > MAX_PORTS) {
        throw std::invalid_argument("Port range exceeds MAX_PORTS");
    }

    for (size_t i = 0; i < total_ports_; ++i) {
        available_ports_.set(i);
    }
}

std::optional<uint16_t> PortPool::Acquire() {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t available_count = available_ports_.count();
    if (available_count == 0) {
        return std::nullopt;
    }

    std::uniform_int_distribution<size_t> dist(0, available_count - 1);
    size_t target_index = dist(rng_);

    size_t current_index = 0;
    for (size_t i = 0; i < total_ports_; ++i) {
        if (available_ports_[i]) {
            if (current_index == target_index) {
                available_ports_.reset(i);
                return min_port_ + static_cast<uint16_t>(i);
            }
            ++current_index;
        }
    }

    return std::nullopt;
}

bool PortPool::Release(uint16_t port) {
    if (port < min_port_ || port > max_port_) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    size_t index = port - min_port_;
    if (!available_ports_[index]) {
        available_ports_.set(index);
        return true;
    }

    return false;
}

size_t PortPool::AvailableCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return available_ports_.count();
}

double PortPool::UsagePercentage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t used = total_ports_ - available_ports_.count();
    return (static_cast<double>(used) / total_ports_) * 100.0;
}

} // namespace relay
} // namespace p2p
