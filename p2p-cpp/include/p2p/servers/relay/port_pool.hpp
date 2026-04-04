/**
 * @file port_pool.hpp
 * @brief Port Pool Management
 *
 * Manages a pool of available relay ports using bitmap for O(1) allocation.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <mutex>
#include <bitset>
#include <random>

namespace p2p {
namespace relay {

/**
 * @brief Port Pool Manager
 *
 * Thread-safe port allocation and deallocation using bitmap.
 * Supports up to 16384 ports (configurable).
 */
class PortPool {
public:
    static constexpr size_t MAX_PORTS = 16384;

    /**
     * @brief Constructor
     * @param min_port Minimum port number
     * @param max_port Maximum port number (inclusive)
     */
    PortPool(uint16_t min_port, uint16_t max_port);

    /**
     * @brief Acquire a port from the pool
     * @return Port number or nullopt if pool exhausted
     */
    std::optional<uint16_t> Acquire();

    /**
     * @brief Release a port back to the pool
     * @param port Port number to release
     * @return true if port was released, false if not in pool
     */
    bool Release(uint16_t port);

    /**
     * @brief Get number of available ports
     */
    size_t AvailableCount() const;

    /**
     * @brief Get port pool usage percentage
     */
    double UsagePercentage() const;

    /**
     * @brief Get total number of ports
     */
    size_t TotalPorts() const { return total_ports_; }

private:
    uint16_t min_port_;
    uint16_t max_port_;
    size_t total_ports_;

    mutable std::mutex mutex_;
    std::bitset<MAX_PORTS> available_ports_;  // 1 = available, 0 = in use
    std::mt19937 rng_;  // Random number generator for load distribution
};

} // namespace relay
} // namespace p2p
