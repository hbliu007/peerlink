#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <chrono>
#include <optional>

namespace p2p {
namespace protocol {

// Forward declarations
class DCUtRSession;
class DCUtRCoordinator;

// DCUtR protocol constants
constexpr const char* DCUTR_PROTOCOL_ID = "/libp2p/dcutr";
constexpr int64_t PUNCH_BUFFER_MS = 100;  // Time buffer for punch coordination

// Address type (multiaddr format)
using Address = std::vector<uint8_t>;

// Connection callback
using ConnectionCallback = std::function<void(bool success, const std::string& error)>;

// DCUtR message types
enum class DCUtRMessageType {
    CONNECT = 0,
    SYNC = 1
};

// CONNECT message data
struct ConnectMessage {
    std::vector<Address> addrs;
    int64_t timestamp_ns;
};

// SYNC message data
struct SyncMessage {
    std::vector<Address> addrs;
    int64_t echo_timestamp_ns;
    int64_t timestamp_ns;
};

// Punch schedule
struct PunchSchedule {
    int64_t punch_time_ns;
    std::vector<Address> target_addrs;
    int64_t rtt_ns;
};

// DCUtR session state
enum class DCUtRState {
    IDLE,
    CONNECT_SENT,
    SYNC_RECEIVED,
    PUNCHING,
    COMPLETED,
    FAILED
};

/**
 * DCUtR Coordinator - handles RTT measurement and punch scheduling
 */
class DCUtRCoordinator {
public:
    DCUtRCoordinator() = default;
    ~DCUtRCoordinator() = default;

    // Get current time in nanoseconds
    int64_t get_current_time_ns();

    // Measure RTT from CONNECT/SYNC exchange
    int64_t measure_rtt(int64_t t1_send, int64_t t4_receive,
                        int64_t t2_receive, int64_t t3_send);

    // Calculate punch schedule for initiator
    PunchSchedule calculate_initiator_schedule(
        int64_t t4_receive,
        int64_t rtt_ns,
        const std::vector<Address>& target_addrs);

    // Calculate punch schedule for responder
    PunchSchedule calculate_responder_schedule(
        int64_t t3_send,
        int64_t rtt_ns,
        const std::vector<Address>& target_addrs);
};

/**
 * DCUtR Session - manages state machine for a single upgrade attempt
 */
class DCUtRSession {
public:
    DCUtRSession(bool is_initiator, const std::string& peer_id);
    ~DCUtRSession() = default;

    // Start the session
    void start(const std::vector<Address>& local_addrs);

    // Handle incoming CONNECT message (responder side)
    void on_connect_received(const ConnectMessage& msg);

    // Handle incoming SYNC message (initiator side)
    void on_sync_received(const SyncMessage& msg);

    // Get current state
    DCUtRState get_state() const { return state_; }

    // Get punch schedule (available after SYNC received)
    std::optional<PunchSchedule> get_punch_schedule() const;

    // Get CONNECT message to send (initiator)
    ConnectMessage get_connect_message() const;

    // Get SYNC message to send (responder)
    SyncMessage get_sync_message() const;

private:
    bool is_initiator_;
    std::string peer_id_;
    DCUtRState state_;
    std::vector<Address> local_addrs_;

    // Timing data
    int64_t t1_connect_sent_;
    int64_t t2_connect_received_;
    int64_t t3_sync_sent_;
    int64_t t4_sync_received_;

    // Coordinator
    DCUtRCoordinator coordinator_;

    // Punch schedule
    std::optional<PunchSchedule> punch_schedule_;
};

/**
 * DCUtR Client - main protocol interface
 */
class DCUtRClient {
public:
    DCUtRClient() = default;
    ~DCUtRClient() = default;

    // Initiate upgrade (initiator side)
    std::shared_ptr<DCUtRSession> InitiateUpgrade(
        const std::string& peer_id,
        const std::vector<Address>& local_addrs);

    // Respond to upgrade (responder side)
    std::shared_ptr<DCUtRSession> RespondToUpgrade(
        const std::string& peer_id,
        const std::vector<Address>& local_addrs,
        const ConnectMessage& connect_msg);

    // Execute coordinated punch
    void ExecuteCoordinatedPunch(
        const PunchSchedule& schedule,
        ConnectionCallback callback);

private:
    // Active sessions
    std::map<std::string, std::shared_ptr<DCUtRSession>> sessions_;
};

} // namespace protocol
} // namespace p2p
