/**
 * @file allocation_manager.hpp
 * @brief TURN Allocation Manager
 *
 * Manages TURN allocations, permissions, and lifecycle.
 */

#pragma once

#include "turn_message.hpp"
#include "port_pool.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <mutex>
#include <thread>

namespace p2p {
namespace relay {

/**
 * @brief TURN Allocation
 *
 * Represents a relay session between client and peers.
 */
class TurnAllocation {
public:
    TurnAllocation(
        std::string allocation_id,
        Address client_addr,
        Address relay_addr,
        uint32_t lifetime,
        TransportProtocol transport = TransportProtocol::UDP);

    // Getters
    const std::string& GetAllocationId() const { return allocation_id_; }
    const Address& GetClientAddr() const { return client_addr_; }
    const Address& GetRelayAddr() const { return relay_addr_; }
    uint32_t GetLifetime() const { return lifetime_; }
    TransportProtocol GetTransport() const { return transport_; }

    // Lifetime management
    bool IsExpired() const;
    uint32_t GetRemainingTime() const;
    void Refresh(uint32_t new_lifetime);

    // Permission management
    bool AddPermission(const Address& peer_addr);
    bool HasPermission(const Address& peer_addr) const;
    void RemovePermission(const Address& peer_addr);

    // Statistics
    void RecordSent(size_t bytes);
    void RecordReceived(size_t bytes);
    uint64_t GetBytesSent() const { return bytes_sent_; }
    uint64_t GetBytesReceived() const { return bytes_received_; }

private:
    std::string allocation_id_;
    Address client_addr_;
    Address relay_addr_;
    uint32_t lifetime_;  // seconds
    TransportProtocol transport_;

    std::chrono::steady_clock::time_point created_at_;
    std::chrono::steady_clock::time_point last_refresh_;

    mutable std::mutex mutex_;
    std::unordered_set<std::string> permissions_;  // peer addresses

    uint64_t bytes_sent_ = 0;
    uint64_t bytes_received_ = 0;
};

/**
 * @brief Allocation Manager
 *
 * Manages all TURN allocations and port pool.
 */
class AllocationManager {
public:
    /**
     * @brief Constructor
     * @param min_port Minimum relay port
     * @param max_port Maximum relay port
     * @param default_lifetime Default allocation lifetime (seconds)
     * @param max_allocations Maximum concurrent allocations
     */
    AllocationManager(
        uint16_t min_port,
        uint16_t max_port,
        uint32_t default_lifetime = 600,
        size_t max_allocations = 1000);

    ~AllocationManager();

    /**
     * @brief Start allocation manager
     */
    void Start();

    /**
     * @brief Stop allocation manager
     */
    void Stop();

    /**
     * @brief Create a new allocation
     * @param client_addr Client address
     * @param relay_ip Relay server IP
     * @param transport Transport protocol
     * @param lifetime Requested lifetime (0 = use default)
     * @return Allocation or nullptr if failed
     */
    std::shared_ptr<TurnAllocation> CreateAllocation(
        const Address& client_addr,
        const std::string& relay_ip,
        TransportProtocol transport = TransportProtocol::UDP,
        uint32_t lifetime = 0);

    /**
     * @brief Get allocation by ID
     */
    std::shared_ptr<TurnAllocation> GetAllocation(const std::string& allocation_id);

    /**
     * @brief Get allocation by client address
     */
    std::shared_ptr<TurnAllocation> GetAllocationByClient(const Address& client_addr);

    /**
     * @brief Get allocation by relay address
     */
    std::shared_ptr<TurnAllocation> GetAllocationByRelay(const Address& relay_addr);

    /**
     * @brief Refresh allocation
     * @param allocation_id Allocation ID
     * @param lifetime New lifetime (0 = use default)
     * @return true if refreshed
     */
    bool RefreshAllocation(const std::string& allocation_id, uint32_t lifetime = 0);

    /**
     * @brief Delete allocation
     * @param allocation_id Allocation ID
     * @return true if deleted
     */
    bool DeleteAllocation(const std::string& allocation_id);

    /**
     * @brief Cleanup expired allocations
     * @return Number of allocations removed
     */
    size_t CleanupExpired();

    /**
     * @brief Get statistics
     */
    struct Stats {
        size_t total_allocations;
        size_t active_allocations;
        size_t max_allocations;
        size_t port_pool_available;
        double port_pool_usage;
        uint64_t total_bytes_sent;
        uint64_t total_bytes_received;
    };
    Stats GetStats() const;

private:
    void CleanupLoop();
    std::string GenerateAllocationId();

    PortPool port_pool_;
    uint32_t default_lifetime_;
    size_t max_allocations_;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<TurnAllocation>> allocations_;
    std::unordered_map<std::string, std::string> client_to_allocation_;  // client_addr -> allocation_id
    std::unordered_map<std::string, std::string> relay_to_allocation_;   // relay_addr -> allocation_id

    bool running_ = false;
    std::thread cleanup_thread_;
};

} // namespace relay
} // namespace p2p
