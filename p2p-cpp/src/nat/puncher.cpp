#include "p2p/nat/puncher.hpp"
#include "p2p/nat/multiaddr_converter.hpp"
#include "p2p/nat/connection.hpp"
#include <chrono>
#include <thread>
#include <algorithm>
#include <random>
#include <cstring>

namespace p2p {
namespace nat {

// Punch packet magic for verification
static constexpr uint8_t PUNCH_MAGIC[] = {
    0x50, 0x32, 0x50, 0x20, 0x50, 0x55, 0x4E, 0x43, 0x48  // "P2P PUNCH"
};

// Constants for hole punching
static constexpr int PUNCH_RETRY_COUNT = 5;
static constexpr int PUNCH_RETRY_DELAY_MS = 50;
static constexpr int PUNCH_RESPONSE_TIMEOUT_MS = 2000;
static constexpr int SIMULTANEOUS_OPEN_ATTEMPTS = 3;
static constexpr int SIMULTANEOUS_OPEN_DELAY_MS = 100;

/**
 * Create punch packet for verification
 */
static std::vector<uint8_t> CreatePunchPacket() {
    std::vector<uint8_t> packet(sizeof(PUNCH_MAGIC));
    std::memcpy(packet.data(), PUNCH_MAGIC, sizeof(PUNCH_MAGIC));
    return packet;
}

/**
 * Verify punch packet
 */
static bool IsPunchPacket(const std::vector<uint8_t>& data) {
    if (data.size() < sizeof(PUNCH_MAGIC)) {
        return false;
    }
    return std::memcmp(data.data(), PUNCH_MAGIC, sizeof(PUNCH_MAGIC)) == 0;
}

// ============================================================================
// UDPPuncher Implementation
// ============================================================================

std::future<PunchResult> UDPPuncher::punch(
    const std::vector<protocol::Address>& target_addrs,
    int64_t punch_time_ns) {

    return std::async(std::launch::async, [this, target_addrs, punch_time_ns]() {
        // Wait until punch time
        auto now_ns = std::chrono::high_resolution_clock::now()
            .time_since_epoch().count();
        int64_t wait_ns = punch_time_ns - now_ns;

        if (wait_ns > 0) {
            std::this_thread::sleep_for(std::chrono::nanoseconds(wait_ns));
        }

        // Try standard punch first
        PunchResult result = StandardPunch(target_addrs);
        if (result.success) {
            return result;
        }

        // Fallback to symmetric punch
        return SymmetricPunch(target_addrs);
    });
}

PunchResult UDPPuncher::StandardPunch(
    const std::vector<protocol::Address>& target_addrs) {

    PunchResult result;
    result.transport_type = "udp";

    if (target_addrs.empty()) {
        result.success = false;
        result.error = "No target addresses provided";
        return result;
    }

    // Parse addresses and try each one
    for (const auto& addr_bytes : target_addrs) {
        auto socket_addr = MultiaddrConverter::parse_udp(addr_bytes);
        if (!socket_addr) {
            continue;  // Skip non-UDP addresses
        }

        // Create UDP socket bound to any available port
        net::UDPSocket socket;
        if (!socket.is_valid()) {
            result.success = false;
            result.error = "Failed to create UDP socket";
            continue;
        }

        // Bind to ephemeral port
        net::SocketAddr local_addr("0.0.0.0", 0);
        if (!socket.bind(local_addr)) {
            result.success = false;
            result.error = "Failed to bind UDP socket";
            continue;
        }

        // Get actual local address
        auto local = socket.get_local_addr();
        if (!local) {
            result.success = false;
            result.error = "Failed to get local address";
            continue;
        }

        // Set socket timeout for receiving
        // Note: We use non-blocking with poll instead
        // For simplicity, we'll do blocking with small timeout

        // Create punch packet
        auto punch_packet = CreatePunchPacket();

        // Send punch packets with retries
        bool punch_success = false;
        std::vector<uint8_t> recv_buffer;
        net::SocketAddr established_peer = *socket_addr;

        for (int attempt = 0; attempt < PUNCH_RETRY_COUNT && !punch_success; ++attempt) {
            // Send punch packet
            ssize_t sent = socket.send_to(punch_packet, *socket_addr);
            if (sent < 0) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(PUNCH_RETRY_DELAY_MS));
                continue;
            }

            // Try to receive response
            net::SocketAddr from_addr;
            recv_buffer.clear();

            // Use non-blocking receive with timeout loop
            auto start = std::chrono::steady_clock::now();
            while (std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - start).count() <
                   PUNCH_RESPONSE_TIMEOUT_MS / PUNCH_RETRY_COUNT) {

                ssize_t received = socket.recv_from(recv_buffer, from_addr);
                if (received > 0) {
                    // Check if it's a punch response from our target
                    if (IsPunchPacket(recv_buffer) && from_addr.ip == socket_addr->ip) {
                        // Accept mapped-port changes from the expected peer IP.
                        established_peer = from_addr;
                        punch_success = true;
                        break;
                    }
                }

                // Small sleep before next receive attempt
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            if (punch_success) {
                break;
            }

            // Wait before next attempt
            std::this_thread::sleep_for(
                std::chrono::milliseconds(PUNCH_RETRY_DELAY_MS));
        }

        if (punch_success) {
            // Create UDP connection using the actual peer address (may differ
            // from target due to NAT port remapping)
            auto connection = std::make_shared<UDPConnection>(
                std::move(socket), established_peer);

            result.success = true;
            result.connection = std::static_pointer_cast<Connection>(connection);
            return result;
        }
    }

    result.success = false;
    result.error = "UDP hole punch failed - no response from peer";
    return result;
}

PunchResult UDPPuncher::SymmetricPunch(
    const std::vector<protocol::Address>& target_addrs) {

    PunchResult result;
    result.transport_type = "udp";
    result.success = false;
    result.error = "Symmetric NAT punch requires port prediction - not implemented";

    // TODO: Implement port prediction for symmetric NAT
    // 1. Use STUN to determine NAT behavior and port allocation pattern
    // 2. Predict next external port
    // 3. Try multiple adjacent ports
    return result;
}

// ============================================================================
// TCPPuncher Implementation
// ============================================================================

std::future<PunchResult> TCPPuncher::punch(
    const std::vector<protocol::Address>& target_addrs,
    int64_t punch_time_ns) {

    return std::async(std::launch::async, [this, target_addrs, punch_time_ns]() {
        // Wait until punch time
        auto now_ns = std::chrono::high_resolution_clock::now()
            .time_since_epoch().count();
        int64_t wait_ns = punch_time_ns - now_ns;

        if (wait_ns > 0) {
            std::this_thread::sleep_for(std::chrono::nanoseconds(wait_ns));
        }

        // Try simultaneous open
        PunchResult result = SimultaneousOpen(target_addrs);
        if (result.success) {
            return result;
        }

        // Fallback to listen mode
        return ListenMode(target_addrs);
    });
}

PunchResult TCPPuncher::SimultaneousOpen(
    const std::vector<protocol::Address>& target_addrs) {

    PunchResult result;
    result.transport_type = "tcp";

    if (target_addrs.empty()) {
        result.success = false;
        result.error = "No target addresses provided";
        return result;
    }

    // Parse addresses and try each one
    for (const auto& addr_bytes : target_addrs) {
        auto socket_addr = MultiaddrConverter::parse_tcp(addr_bytes);
        if (!socket_addr) {
            continue;  // Skip non-TCP addresses
        }

        // TCP simultaneous open requires precise timing
        // Both peers must call connect() at approximately the same time
        // This works for Full Cone and Restricted Cone NATs

        // Create TCP socket
        net::TCPSocket socket;
        if (!socket.IsValid()) {
            result.success = false;
            result.error = "Failed to create TCP socket";
            continue;
        }

        // Bind to ephemeral port
        net::SocketAddr local_addr("0.0.0.0", 0);
        if (!socket.Bind(local_addr)) {
            result.success = false;
            result.error = "Failed to bind TCP socket";
            continue;
        }

        // Enable SO_REUSEADDR for faster retry
        // (already enabled in TCPSocket constructor)

        // Attempt simultaneous open with retries
        bool connected = false;

        for (int attempt = 0; attempt < SIMULTANEOUS_OPEN_ATTEMPTS; ++attempt) {
            // Initiate connection (non-blocking)
            if (socket.Connect(*socket_addr)) {
                // Give time for connection to establish
                // In simultaneous open, both sides send SYN at same time
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(SIMULTANEOUS_OPEN_DELAY_MS));

                // Check if connected
                if (socket.IsConnected()) {
                    connected = true;
                    break;
                }
            }

            // Small delay before retry
            std::this_thread::sleep_for(
                std::chrono::milliseconds(SIMULTANEOUS_OPEN_DELAY_MS));
        }

        if (connected) {
            // Create TCP connection
            auto connection = std::make_shared<TCPConnection>(std::move(socket));

            result.success = true;
            result.connection = std::static_pointer_cast<Connection>(connection);
            return result;
        }
    }

    result.success = false;
    result.error = "TCP simultaneous open failed - connection not established";
    return result;
}

PunchResult TCPPuncher::ListenMode(
    const std::vector<protocol::Address>& target_addrs) {

    PunchResult result;
    result.transport_type = "tcp";

    if (target_addrs.empty()) {
        result.success = false;
        result.error = "No target addresses provided";
        return result;
    }

    // For listen mode, we create a listening socket and try to connect
    // This is a fallback when simultaneous open fails
    // It works when one peer is behind a port-restricted NAT

    // Parse local TCP address from targets (if available)
    // For now, bind to ephemeral port
    net::TCPSocket listen_socket;
    if (!listen_socket.is_valid()) {
        result.success = false;
        result.error = "Failed to create listen socket";
        return result;
    }

    // Bind to ephemeral port
    net::SocketAddr local_addr("0.0.0.0", 0);
    if (!listen_socket.bind(local_addr)) {
        result.success = false;
        result.error = "Failed to bind listen socket";
        return result;
    }

    // Start listening
    if (!listen_socket.listen(5)) {
        result.success = false;
        result.error = "Failed to listen";
        return result;
    }

    auto bound_addr = listen_socket.get_local_addr();
    if (!bound_addr) {
        result.success = false;
        result.error = "Failed to get bound address";
        return result;
    }

    // Try to connect to each target
    for (const auto& addr_bytes : target_addrs) {
        auto socket_addr = MultiaddrConverter::parse_tcp(addr_bytes);
        if (!socket_addr) {
            continue;
        }

        // Create outbound socket
        net::TCPSocket client_socket;
        if (!client_socket.is_valid()) {
            continue;
        }

        if (!client_socket.bind(local_addr)) {
            continue;
        }

        // Attempt connection
        if (client_socket.connect(*socket_addr)) {
            // Wait for connection
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            if (client_socket.is_connected()) {
                auto connection = std::make_shared<TCPConnection>(
                    std::move(client_socket));

                result.success = true;
                result.connection = std::static_pointer_cast<Connection>(connection);
                return result;
            }
        }
    }

    // Check for incoming connections on listen socket
    // (brief poll - in production, use proper event loop)
    auto deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(1000);

    while (std::chrono::steady_clock::now() < deadline) {
        net::SocketAddr peer_addr;
        auto accepted = listen_socket.accept(peer_addr);
        if (accepted && accepted->is_connected()) {
            auto connection = std::make_shared<TCPConnection>(
                std::move(*accepted));

            result.success = true;
            result.connection = std::static_pointer_cast<Connection>(connection);
            return result;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    result.success = false;
    result.error = "TCP listen mode - no incoming connection";
    return result;
}

// ============================================================================
// NATTraversalCoordinator Implementation
// ============================================================================

NATTraversalCoordinator::NATTraversalCoordinator()
    : udp_puncher_(std::make_unique<UDPPuncher>()),
      tcp_puncher_(std::make_unique<TCPPuncher>()) {
}

void NATTraversalCoordinator::execute_coordinated_punch(
    const protocol::PunchSchedule& schedule,
    PunchCallback callback) {

    // Wait until punch time
    WaitUntilPunchTime(schedule.punch_time_ns);

    // Launch UDP and TCP punch in parallel
    auto udp_future = udp_puncher_->punch(
        schedule.target_addrs, schedule.punch_time_ns);
    auto tcp_future = tcp_puncher_->punch(
        schedule.target_addrs, schedule.punch_time_ns);

    // Wait for both to complete
    std::vector<PunchResult> results;
    results.push_back(udp_future.get());
    results.push_back(tcp_future.get());

    // Select best result
    PunchResult best = SelectBest(results);
    callback(best);
}

void NATTraversalCoordinator::execute_with_relay_fallback(
    const protocol::PunchSchedule& schedule,
    std::shared_ptr<Connection> relay_connection,
    PunchCallback callback) {

    execute_coordinated_punch(schedule, [relay_connection, callback](const PunchResult& result) {
        if (result.success) {
            // Direct connection succeeded
            callback(result);
        } else {
            // Fallback to relay
            PunchResult relay_result;
            relay_result.success = true;
            relay_result.transport_type = "relay";
            relay_result.connection = relay_connection;
            callback(relay_result);
        }
    });
}

void NATTraversalCoordinator::WaitUntilPunchTime(int64_t punch_time_ns) {
    auto now_ns = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    int64_t wait_ns = punch_time_ns - now_ns;

    if (wait_ns > 0) {
        std::this_thread::sleep_for(std::chrono::nanoseconds(wait_ns));
    }
}

PunchResult NATTraversalCoordinator::SelectBest(
    const std::vector<PunchResult>& results) {

    // Priority: UDP > TCP
    for (const auto& result : results) {
        if (result.success && result.transport_type == "udp") {
            return result;
        }
    }

    for (const auto& result : results) {
        if (result.success && result.transport_type == "tcp") {
            return result;
        }
    }

    // All failed, return first failure
    if (!results.empty()) {
        return results[0];
    }

    PunchResult failure;
    failure.success = false;
    failure.error = "No punch attempts made";
    return failure;
}

} // namespace nat
} // namespace p2p
