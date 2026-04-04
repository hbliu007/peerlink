/**
 * @file allocation_manager.cpp
 * @brief Allocation Manager Implementation
 */

#include "p2p/servers/relay/allocation_manager.hpp"

#include <chrono>
#include <iomanip>
#include <random>
#include <sstream>
#include <thread>

namespace p2p {
namespace relay {

// TurnAllocation Implementation

TurnAllocation::TurnAllocation(
    std::string allocation_id,
    Address client_addr,
    Address relay_addr,
    uint32_t lifetime,
    TransportProtocol transport)
    : allocation_id_(std::move(allocation_id)),
      client_addr_(std::move(client_addr)),
      relay_addr_(std::move(relay_addr)),
      lifetime_(lifetime),
      transport_(transport),
      created_at_(std::chrono::steady_clock::now()),
      last_refresh_(std::chrono::steady_clock::now()) {
}

bool TurnAllocation::IsExpired() const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_refresh_).count();
    return elapsed >= lifetime_;
}

uint32_t TurnAllocation::GetRemainingTime() const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_refresh_).count();
    int32_t remaining = lifetime_ - elapsed;
    return remaining > 0 ? static_cast<uint32_t>(remaining) : 0;
}

void TurnAllocation::Refresh(uint32_t new_lifetime) {
    std::lock_guard<std::mutex> lock(mutex_);
    lifetime_ = new_lifetime;
    last_refresh_ = std::chrono::steady_clock::now();
}

bool TurnAllocation::AddPermission(const Address& peer_addr) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto result = permissions_.insert(peer_addr.ToString());
    return result.second;
}

bool TurnAllocation::HasPermission(const Address& peer_addr) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return permissions_.count(peer_addr.ToString()) > 0;
}

void TurnAllocation::RemovePermission(const Address& peer_addr) {
    std::lock_guard<std::mutex> lock(mutex_);
    permissions_.erase(peer_addr.ToString());
}

void TurnAllocation::RecordSent(size_t bytes) {
    bytes_sent_ += bytes;
}

void TurnAllocation::RecordReceived(size_t bytes) {
    bytes_received_ += bytes;
}

// AllocationManager Implementation

AllocationManager::AllocationManager(
    uint16_t min_port,
    uint16_t max_port,
    uint32_t default_lifetime,
    size_t max_allocations)
    : port_pool_(min_port, max_port),
      default_lifetime_(default_lifetime),
      max_allocations_(max_allocations) {
}

AllocationManager::~AllocationManager() {
    Stop();
}

void AllocationManager::Start() {
    running_ = true;
    cleanup_thread_ = std::thread(&AllocationManager::CleanupLoop, this);
}

void AllocationManager::Stop() {
    running_ = false;
    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
    }
}

std::string AllocationManager::GenerateAllocationId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;

    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(16) << dis(gen)
        << std::setw(16) << dis(gen);
    return oss.str();
}

std::shared_ptr<TurnAllocation> AllocationManager::CreateAllocation(
    const Address& client_addr,
    const std::string& relay_ip,
    TransportProtocol transport,
    uint32_t lifetime) {

    std::string client_key = client_addr.ToString();

    std::lock_guard<std::mutex> lock(mutex_);

    // Check allocation limit
    if (allocations_.size() >= max_allocations_) {
        return nullptr;
    }

    // Check if client already has allocation
    if (client_to_allocation_.count(client_key) > 0) {
        return nullptr;
    }

    // Allocate port
    auto port = port_pool_.Acquire();
    if (!port) {
        return nullptr;
    }

    // Create allocation
    std::string allocation_id = GenerateAllocationId();
    Address relay_addr(relay_ip, *port);
    std::string relay_key = relay_addr.ToString();
    uint32_t actual_lifetime = lifetime > 0 ? lifetime : default_lifetime_;

    auto allocation = std::make_shared<TurnAllocation>(
        allocation_id,
        client_addr,
        relay_addr,
        actual_lifetime,
        transport);

    // Store allocation
    allocations_[allocation_id] = allocation;
    client_to_allocation_[std::move(client_key)] = allocation_id;
    relay_to_allocation_[std::move(relay_key)] = allocation_id;

    return allocation;
}

std::shared_ptr<TurnAllocation> AllocationManager::GetAllocation(
    const std::string& allocation_id) {

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = allocations_.find(allocation_id);
    return it != allocations_.end() ? it->second : nullptr;
}

std::shared_ptr<TurnAllocation> AllocationManager::GetAllocationByClient(
    const Address& client_addr) {

    std::string client_key = client_addr.ToString();
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = client_to_allocation_.find(client_key);
    if (it != client_to_allocation_.end()) {
        auto alloc_it = allocations_.find(it->second);
        if (alloc_it != allocations_.end()) {
            return alloc_it->second;
        }
    }
    return nullptr;
}

std::shared_ptr<TurnAllocation> AllocationManager::GetAllocationByRelay(
    const Address& relay_addr) {

    std::string relay_key = relay_addr.ToString();
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = relay_to_allocation_.find(relay_key);
    if (it != relay_to_allocation_.end()) {
        auto alloc_it = allocations_.find(it->second);
        if (alloc_it != allocations_.end()) {
            return alloc_it->second;
        }
    }
    return nullptr;
}

bool AllocationManager::RefreshAllocation(
    const std::string& allocation_id,
    uint32_t lifetime) {

    auto allocation = GetAllocation(allocation_id);
    if (!allocation) {
        return false;
    }

    uint32_t actual_lifetime = lifetime > 0 ? lifetime : default_lifetime_;
    allocation->Refresh(actual_lifetime);
    return true;
}

bool AllocationManager::DeleteAllocation(const std::string& allocation_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = allocations_.find(allocation_id);
    if (it == allocations_.end()) {
        return false;
    }

    auto allocation = it->second;

    // Cache address strings to avoid repeated to_string() calls
    std::string client_key = allocation->GetClientAddr().ToString();
    std::string relay_key = allocation->GetRelayAddr().ToString();

    // Release port
    port_pool_.Release(allocation->GetRelayAddr().port);

    // Remove from indexes
    client_to_allocation_.erase(client_key);
    relay_to_allocation_.erase(relay_key);
    allocations_.erase(it);

    return true;
}

size_t AllocationManager::CleanupExpired() {
    std::vector<std::string> expired_ids;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        expired_ids.reserve(allocations_.size() / 10);  // Reserve space to avoid reallocation
        for (const auto& [id, allocation] : allocations_) {
            if (allocation->IsExpired()) {
                expired_ids.push_back(id);
            }
        }
    }

    // Delete outside of iteration to avoid iterator invalidation
    size_t deleted = 0;
    for (const auto& id : expired_ids) {
        if (DeleteAllocation(id)) {
            deleted++;
        }
    }

    return deleted;
}

void AllocationManager::CleanupLoop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(60));
        CleanupExpired();
    }
}

AllocationManager::Stats AllocationManager::GetStats() const {
    std::lock_guard<std::mutex> lock(mutex_);

    Stats stats;
    stats.total_allocations = allocations_.size();
    stats.max_allocations = max_allocations_;
    stats.port_pool_available = port_pool_.AvailableCount();
    stats.port_pool_usage = port_pool_.UsagePercentage();

    stats.active_allocations = 0;
    stats.total_bytes_sent = 0;
    stats.total_bytes_received = 0;

    for (const auto& [id, allocation] : allocations_) {
        if (!allocation->IsExpired()) {
            stats.active_allocations++;
            stats.total_bytes_sent += allocation->GetBytesSent();
            stats.total_bytes_received += allocation->GetBytesReceived();
        }
    }

    return stats;
}

} // namespace relay
} // namespace p2p
