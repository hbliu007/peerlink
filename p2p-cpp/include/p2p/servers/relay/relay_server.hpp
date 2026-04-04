/**
 * @file relay_server.hpp
 * @brief TURN Relay Server
 *
 * Implements TURN protocol server for P2P data relay.
 */

#pragma once

#include "turn_message.hpp"
#include "allocation_manager.hpp"
#include "bandwidth_limiter.hpp"
#include "rate_limiter.hpp"
#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <boost/asio.hpp>

namespace asio = boost::asio;

namespace p2p {
namespace relay {

/**
 * @brief Relay Server Configuration
 */
struct RelayServerConfig {
    std::string host = "0.0.0.0";
    uint16_t port = 9001;  // Control port
    std::string public_ip = "127.0.0.1";

    uint16_t min_port = 50000;
    uint16_t max_port = 50100;

    uint32_t default_lifetime = 600;  // seconds
    size_t max_allocations = 1000;

    BandwidthLimit bandwidth_limit;
    RateLimitConfig rate_limit;
};

/**
 * @brief TURN Relay Server
 *
 * Handles allocation requests and relays data between clients and peers.
 */
class RelayServer {
public:
    /**
     * @brief Constructor
     * @param config Server configuration
     */
    explicit RelayServer(const RelayServerConfig& config);

    /**
     * @brief Destructor
     */
    ~RelayServer();

    /**
     * @brief Start relay server
     */
    void Start();

    /**
     * @brief Stop relay server
     */
    void Stop();

    /**
     * @brief Get server statistics
     */
    struct Stats {
        AllocationManager::Stats allocations;
        BandwidthLimiter::GlobalStats bandwidth;
        RateLimiter::Stats rate_limit;
        size_t relay_sockets;
    };
    Stats GetStats() const;

private:
    // Control channel handlers
    void StartControlChannel();
    void ControlLoop();
    void HandleControlMessage(
        const uint8_t* data,
        size_t len,
        const asio::ip::udp::endpoint& client_endpoint);

    // TURN message handlers
    std::vector<uint8_t> HandleBindingRequest(
        const StunMessage& request,
        const Address& client_addr);

    std::vector<uint8_t> HandleAllocateRequest(
        const StunMessage& request,
        const Address& client_addr);

    std::vector<uint8_t> HandleRefreshRequest(
        const StunMessage& request,
        const Address& client_addr);

    std::vector<uint8_t> HandleCreatePermissionRequest(
        const StunMessage& request,
        const Address& client_addr);

    void HandleSendIndication(
        const StunMessage& request,
        const Address& client_addr);

    // Relay socket management
    void CreateRelaySocket(std::shared_ptr<TurnAllocation> allocation);
    void RelayLoop(
        std::shared_ptr<TurnAllocation> allocation,
        std::shared_ptr<asio::ip::udp::socket> socket);
    void SendDataIndication(
        std::shared_ptr<TurnAllocation> allocation,
        const asio::ip::udp::endpoint& peer_endpoint,
        const std::vector<uint8_t>& data);

    RelayServerConfig config_;

    std::unique_ptr<AllocationManager> allocation_manager_;
    std::unique_ptr<BandwidthLimiter> bandwidth_limiter_;
    std::unique_ptr<RateLimiter> rate_limiter_;
    std::unique_ptr<ThroughputMonitor> throughput_monitor_;

    std::atomic<bool> running_{false};

    // ASIO
    std::unique_ptr<asio::io_context> io_context_;
    std::unique_ptr<asio::ip::udp::socket> control_socket_;
    std::vector<std::thread> io_threads_;

    // Relay sockets
    mutable std::mutex relay_sockets_mutex_;
    std::unordered_map<uint16_t, std::shared_ptr<asio::ip::udp::socket>> relay_sockets_;
};

} // namespace relay
} // namespace p2p
