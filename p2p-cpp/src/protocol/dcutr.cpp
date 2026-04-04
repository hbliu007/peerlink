#include "p2p/protocol/dcutr.hpp"
#include <chrono>
#include <thread>
#include <stdexcept>

namespace p2p {
namespace protocol {

// ============================================================================
// DCUtRCoordinator Implementation
// ============================================================================

int64_t DCUtRCoordinator::get_current_time_ns() {
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();
}

int64_t DCUtRCoordinator::measure_rtt(
    int64_t t1_send,
    int64_t t4_receive,
    int64_t t2_receive,
    int64_t t3_send) {

    // RTT = (t4 - t1) - (t3 - t2)
    // Assuming processing time is negligible, we can simplify to:
    // RTT ≈ (t4 - t1)
    int64_t rtt = t4_receive - t1_send;

    // Sanity check
    if (rtt < 0) {
        throw std::runtime_error("Invalid RTT: negative value");
    }

    return rtt;
}

PunchSchedule DCUtRCoordinator::calculate_initiator_schedule(
    int64_t t4_receive,
    int64_t rtt_ns,
    const std::vector<Address>& target_addrs) {

    PunchSchedule schedule;
    schedule.rtt_ns = rtt_ns;
    schedule.target_addrs = target_addrs;

    // punch_time = t4 + RTT + buffer
    int64_t buffer_ns = PUNCH_BUFFER_MS * 1000000LL;  // Convert ms to ns
    schedule.punch_time_ns = t4_receive + rtt_ns + buffer_ns;

    return schedule;
}

PunchSchedule DCUtRCoordinator::calculate_responder_schedule(
    int64_t t3_send,
    int64_t rtt_ns,
    const std::vector<Address>& target_addrs) {

    PunchSchedule schedule;
    schedule.rtt_ns = rtt_ns;
    schedule.target_addrs = target_addrs;

    // punch_time = t3 + RTT + buffer
    int64_t buffer_ns = PUNCH_BUFFER_MS * 1000000LL;
    schedule.punch_time_ns = t3_send + rtt_ns + buffer_ns;

    return schedule;
}

// ============================================================================
// DCUtRSession Implementation
// ============================================================================

DCUtRSession::DCUtRSession(bool is_initiator, const std::string& peer_id)
    : is_initiator_(is_initiator),
      peer_id_(peer_id),
      state_(DCUtRState::IDLE),
      t1_connect_sent_(0),
      t2_connect_received_(0),
      t3_sync_sent_(0),
      t4_sync_received_(0) {
}

void DCUtRSession::start(const std::vector<Address>& local_addrs) {
    if (state_ != DCUtRState::IDLE) {
        throw std::runtime_error("Session already started");
    }

    local_addrs_ = local_addrs;

    if (is_initiator_) {
        // Initiator: prepare to send CONNECT
        t1_connect_sent_ = coordinator_.get_current_time_ns();
        state_ = DCUtRState::CONNECT_SENT;
    }
}

void DCUtRSession::on_connect_received(const ConnectMessage& msg) {
    if (!is_initiator_) {
        // Responder receives CONNECT
        t2_connect_received_ = coordinator_.get_current_time_ns();

        // Prepare SYNC response
        t3_sync_sent_ = coordinator_.get_current_time_ns();
        state_ = DCUtRState::SYNC_RECEIVED;

        // Calculate punch schedule for responder
        // Note: We don't have full RTT yet, estimate based on one-way delay
        int64_t estimated_rtt = (t3_sync_sent_ - msg.timestamp_ns) * 2;
        punch_schedule_ = coordinator_.calculate_responder_schedule(
            t3_sync_sent_, estimated_rtt, msg.addrs);
    }
}

void DCUtRSession::on_sync_received(const SyncMessage& msg) {
    if (is_initiator_) {
        // Initiator receives SYNC
        t4_sync_received_ = coordinator_.get_current_time_ns();

        // Measure RTT
        int64_t rtt = coordinator_.measure_rtt(
            t1_connect_sent_, t4_sync_received_,
            msg.echo_timestamp_ns, msg.timestamp_ns);

        // Calculate punch schedule for initiator
        punch_schedule_ = coordinator_.calculate_initiator_schedule(
            t4_sync_received_, rtt, msg.addrs);

        state_ = DCUtRState::PUNCHING;
    }
}

std::optional<PunchSchedule> DCUtRSession::get_punch_schedule() const {
    return punch_schedule_;
}

ConnectMessage DCUtRSession::get_connect_message() const {
    if (!is_initiator_ || state_ != DCUtRState::CONNECT_SENT) {
        throw std::runtime_error("Cannot get CONNECT message in current state");
    }

    ConnectMessage msg;
    msg.addrs = local_addrs_;
    msg.timestamp_ns = t1_connect_sent_;
    return msg;
}

SyncMessage DCUtRSession::get_sync_message() const {
    if (is_initiator_ || state_ != DCUtRState::SYNC_RECEIVED) {
        throw std::runtime_error("Cannot get SYNC message in current state");
    }

    SyncMessage msg;
    msg.addrs = local_addrs_;
    msg.echo_timestamp_ns = t2_connect_received_;
    msg.timestamp_ns = t3_sync_sent_;
    return msg;
}

// ============================================================================
// DCUtRClient Implementation
// ============================================================================

std::shared_ptr<DCUtRSession> DCUtRClient::InitiateUpgrade(
    const std::string& peer_id,
    const std::vector<Address>& local_addrs) {

    auto session = std::make_shared<DCUtRSession>(true, peer_id);
    session->start(local_addrs);

    sessions_[peer_id] = session;
    return session;
}

std::shared_ptr<DCUtRSession> DCUtRClient::RespondToUpgrade(
    const std::string& peer_id,
    const std::vector<Address>& local_addrs,
    const ConnectMessage& connect_msg) {

    auto session = std::make_shared<DCUtRSession>(false, peer_id);
    session->start(local_addrs);  // Set local addresses first
    session->on_connect_received(connect_msg);

    sessions_[peer_id] = session;
    return session;
}

void DCUtRClient::ExecuteCoordinatedPunch(
    const PunchSchedule& schedule,
    ConnectionCallback callback) {

    // Get current time
    auto now_ns = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();

    // Calculate wait time
    int64_t wait_ns = schedule.punch_time_ns - now_ns;

    if (wait_ns > 0) {
        // Sleep until punch time
        std::this_thread::sleep_for(
            std::chrono::nanoseconds(wait_ns));
    }

    // TODO: Execute actual TCP/UDP punch
    // This will be implemented in the NAT traversal integration
    callback(false, "not implemented");
}

} // namespace protocol
} // namespace p2p
