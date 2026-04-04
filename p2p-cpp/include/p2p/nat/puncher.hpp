#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <future>
#include "p2p/protocol/dcutr.hpp"
#include "p2p/nat/connection.hpp"

namespace p2p {
namespace nat {

// Connection class is now defined in connection.hpp

// Connection result
struct PunchResult {
    bool success;
    std::string transport_type;  // "udp" or "tcp"
    std::shared_ptr<Connection> connection;
    std::string error;
};

// Connection callback
using PunchCallback = std::function<void(const PunchResult&)>;

/**
 * Base class for hole punching
 */
class HolePuncher {
public:
    virtual ~HolePuncher() = default;

    // Execute hole punch
    virtual std::future<PunchResult> punch(
        const std::vector<protocol::Address>& target_addrs,
        int64_t punch_time_ns) = 0;

    // Get transport type
    virtual std::string get_transport_type() const = 0;
};

/**
 * UDP Hole Puncher
 */
class UDPPuncher : public HolePuncher {
public:
    UDPPuncher() = default;
    ~UDPPuncher() override = default;

    std::future<PunchResult> punch(
        const std::vector<protocol::Address>& target_addrs,
        int64_t punch_time_ns) override;

    std::string get_transport_type() const override { return "udp"; }

private:
    // Standard UDP punch (for Full Cone, Restricted, Port Restricted NAT)
    PunchResult StandardPunch(const std::vector<protocol::Address>& target_addrs);

    // Symmetric NAT punch (requires port prediction)
    PunchResult SymmetricPunch(const std::vector<protocol::Address>& target_addrs);
};

/**
 * TCP Hole Puncher
 */
class TCPPuncher : public HolePuncher {
public:
    TCPPuncher() = default;
    ~TCPPuncher() override = default;

    std::future<PunchResult> punch(
        const std::vector<protocol::Address>& target_addrs,
        int64_t punch_time_ns) override;

    std::string get_transport_type() const override { return "tcp"; }

private:
    // TCP Simultaneous Open
    PunchResult SimultaneousOpen(const std::vector<protocol::Address>& target_addrs);

    // TCP Listen mode (fallback)
    PunchResult ListenMode(const std::vector<protocol::Address>& target_addrs);
};

/**
 * NAT Traversal Coordinator
 * Integrates DCUtR protocol with hole punching
 */
class NATTraversalCoordinator {
public:
    NATTraversalCoordinator();
    ~NATTraversalCoordinator() = default;

    // Execute coordinated punch using DCUtR schedule
    void execute_coordinated_punch(
        const protocol::PunchSchedule& schedule,
        PunchCallback callback);

    // Execute coordinated punch with fallback to relay
    void execute_with_relay_fallback(
        const protocol::PunchSchedule& schedule,
        std::shared_ptr<Connection> relay_connection,
        PunchCallback callback);

private:
    std::unique_ptr<UDPPuncher> udp_puncher_;
    std::unique_ptr<TCPPuncher> tcp_puncher_;

    // Wait until punch time
    void WaitUntilPunchTime(int64_t punch_time_ns);

    // Select best result from multiple punch attempts
    PunchResult SelectBest(const std::vector<PunchResult>& results);
};

} // namespace nat
} // namespace p2p
