#include <gtest/gtest.h>
#include "p2p/protocol/dcutr.hpp"
#include "p2p/nat/puncher.hpp"
#include <thread>
#include <chrono>
#include <atomic>

using namespace p2p::protocol;
using namespace p2p::nat;

namespace {

class RelayTestConnection : public Connection {
public:
    explicit RelayTestConnection(std::string type) : type_(std::move(type)), connected_(true) {}

    std::string get_type() const override { return type_; }
    bool is_connected() const override { return connected_; }
    ssize_t send(const std::vector<uint8_t>& data) override {
        return static_cast<ssize_t>(data.size());
    }
    ssize_t recv(std::vector<uint8_t>&, size_t) override { return 0; }
    void close() override { connected_ = false; }
    std::optional<p2p::net::SocketAddr> get_local_addr() const override { return std::nullopt; }
    std::optional<p2p::net::SocketAddr> get_remote_addr() const override { return std::nullopt; }

private:
    std::string type_;
    bool connected_;
};

} // namespace

// DCUtR Integration Tests

class DCUtRIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        initiator_client = std::make_unique<DCUtRClient>();
        responder_client = std::make_unique<DCUtRClient>();
        coordinator = std::make_unique<NATTraversalCoordinator>();
    }

    std::unique_ptr<DCUtRClient> initiator_client;
    std::unique_ptr<DCUtRClient> responder_client;
    std::unique_ptr<NATTraversalCoordinator> coordinator;

    std::vector<Address> initiator_addrs = {{192, 168, 1, 100}, {203, 0, 113, 50}};
    std::vector<Address> responder_addrs = {{192, 168, 1, 200}, {198, 51, 100, 75}};
};

TEST_F(DCUtRIntegrationTest, EndToEndUpgrade) {
    // Step 1: Initiator starts upgrade
    std::string peer_id = "responder-peer-123";
    auto initiator_session = initiator_client->InitiateUpgrade(peer_id, initiator_addrs);

    ASSERT_NE(initiator_session, nullptr);
    EXPECT_EQ(initiator_session->get_state(), DCUtRState::CONNECT_SENT);

    // Step 2: Get CONNECT message
    ConnectMessage connect_msg = initiator_session->get_connect_message();
    EXPECT_EQ(connect_msg.addrs.size(), 2);
    EXPECT_GT(connect_msg.timestamp_ns, 0);

    // Step 3: Responder receives CONNECT
    auto responder_session = responder_client->RespondToUpgrade(
        "initiator-peer-456", responder_addrs, connect_msg);

    ASSERT_NE(responder_session, nullptr);
    EXPECT_EQ(responder_session->get_state(), DCUtRState::SYNC_RECEIVED);

    // Step 4: Get SYNC message
    SyncMessage sync_msg = responder_session->get_sync_message();
    EXPECT_GT(sync_msg.echo_timestamp_ns, 0);
    EXPECT_GT(sync_msg.timestamp_ns, 0);

    // Step 5: Initiator receives SYNC
    initiator_session->on_sync_received(sync_msg);
    EXPECT_EQ(initiator_session->get_state(), DCUtRState::PUNCHING);

    // Step 6: Both sides should have punch schedules
    auto initiator_schedule = initiator_session->get_punch_schedule();
    auto responder_schedule = responder_session->get_punch_schedule();

    ASSERT_TRUE(initiator_schedule.has_value());
    ASSERT_TRUE(responder_schedule.has_value());

    // Verify schedules are reasonable
    EXPECT_GT(initiator_schedule->punch_time_ns, 0);
    EXPECT_GT(responder_schedule->punch_time_ns, 0);
    EXPECT_GT(initiator_schedule->rtt_ns, 0);
}

TEST_F(DCUtRIntegrationTest, CoordinatedPunchBothSides) {
    // Setup: Complete DCUtR handshake
    auto initiator_session = initiator_client->InitiateUpgrade(
        "responder", initiator_addrs);
    ConnectMessage connect_msg = initiator_session->get_connect_message();

    auto responder_session = responder_client->RespondToUpgrade(
        "initiator", responder_addrs, connect_msg);
    SyncMessage sync_msg = responder_session->get_sync_message();

    initiator_session->on_sync_received(sync_msg);

    auto initiator_schedule = initiator_session->get_punch_schedule().value();
    auto responder_schedule = responder_session->get_punch_schedule().value();

    // Adjust punch times to be in the near future
    int64_t now = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    initiator_schedule.punch_time_ns = now + 50000000LL;  // +50ms
    responder_schedule.punch_time_ns = now + 50000000LL;  // +50ms

    // Execute coordinated punch on both sides
    std::atomic<bool> initiator_done{false};
    std::atomic<bool> responder_done{false};
    PunchResult initiator_result;
    PunchResult responder_result;

    // Initiator punch
    std::thread initiator_thread([&]() {
        coordinator->execute_coordinated_punch(initiator_schedule,
            [&](const PunchResult& result) {
                initiator_result = result;
                initiator_done = true;
            });
    });

    // Responder punch
    std::thread responder_thread([&]() {
        coordinator->execute_coordinated_punch(responder_schedule,
            [&](const PunchResult& result) {
                responder_result = result;
                responder_done = true;
            });
    });

    // Wait for both to complete
    initiator_thread.join();
    responder_thread.join();

    // Callbacks run; without real peers / valid multiaddrs, hole punch does not succeed in CI.
    EXPECT_TRUE(initiator_done);
    EXPECT_TRUE(responder_done);
    EXPECT_FALSE(initiator_result.success);
    EXPECT_FALSE(responder_result.success);
}

TEST_F(DCUtRIntegrationTest, RelayFallbackOnPunchFailure) {
    // Setup with empty addresses (will cause punch to fail)
    auto initiator_session = initiator_client->InitiateUpgrade(
        "responder", {});  // Empty addresses
    ConnectMessage connect_msg = initiator_session->get_connect_message();

    auto responder_session = responder_client->RespondToUpgrade(
        "initiator", {}, connect_msg);  // Empty addresses for responder too
    SyncMessage sync_msg = responder_session->get_sync_message();

    initiator_session->on_sync_received(sync_msg);

    auto schedule = initiator_session->get_punch_schedule().value();

    // Create relay connection
    auto relay_conn = std::make_shared<RelayTestConnection>("relay");

    // Execute with relay fallback
    bool callback_called = false;
    PunchResult result;

    coordinator->execute_with_relay_fallback(schedule, relay_conn,
        [&](const PunchResult& r) {
            callback_called = true;
            result = r;
        });

    EXPECT_TRUE(callback_called);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.transport_type, "relay");
    EXPECT_EQ(result.connection, relay_conn);
}

TEST_F(DCUtRIntegrationTest, RTTMeasurementAccuracy) {
    // Create sessions with known timing
    auto initiator_session = initiator_client->InitiateUpgrade(
        "responder", initiator_addrs);

    // Get CONNECT with timestamp
    ConnectMessage connect_msg = initiator_session->get_connect_message();
    (void)connect_msg.timestamp_ns;

    // Simulate network delay (50ms)
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Responder receives CONNECT
    auto responder_session = responder_client->RespondToUpgrade(
        "initiator", responder_addrs, connect_msg);

    // Get SYNC
    SyncMessage sync_msg = responder_session->get_sync_message();

    // Simulate return delay (50ms)
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Initiator receives SYNC
    initiator_session->on_sync_received(sync_msg);

    // Check RTT measurement
    auto schedule = initiator_session->get_punch_schedule().value();
    int64_t measured_rtt_ms = schedule.rtt_ns / 1000000;

    // RTT should be approximately 100ms (50ms each way)
    // Allow some tolerance for processing time
    EXPECT_GT(measured_rtt_ms, 80);
    EXPECT_LT(measured_rtt_ms, 150);
}

TEST_F(DCUtRIntegrationTest, MultipleSimultaneousSessions) {
    // Test multiple concurrent DCUtR sessions
    const int num_sessions = 5;
    std::vector<std::shared_ptr<DCUtRSession>> initiator_sessions;
    std::vector<std::shared_ptr<DCUtRSession>> responder_sessions;

    // Create multiple sessions
    for (int i = 0; i < num_sessions; i++) {
        std::string peer_id = "peer-" + std::to_string(i);

        auto init_session = initiator_client->InitiateUpgrade(
            peer_id, initiator_addrs);
        initiator_sessions.push_back(init_session);

        ConnectMessage connect_msg = init_session->get_connect_message();

        auto resp_session = responder_client->RespondToUpgrade(
            "initiator-" + std::to_string(i), responder_addrs, connect_msg);
        responder_sessions.push_back(resp_session);

        SyncMessage sync_msg = resp_session->get_sync_message();
        init_session->on_sync_received(sync_msg);
    }

    // Verify all sessions are in correct state
    for (int i = 0; i < num_sessions; i++) {
        EXPECT_EQ(initiator_sessions[i]->get_state(), DCUtRState::PUNCHING);
        EXPECT_EQ(responder_sessions[i]->get_state(), DCUtRState::SYNC_RECEIVED);

        EXPECT_TRUE(initiator_sessions[i]->get_punch_schedule().has_value());
        EXPECT_TRUE(responder_sessions[i]->get_punch_schedule().has_value());
    }
}

TEST_F(DCUtRIntegrationTest, PunchScheduleTimingCoordination) {
    // Verify that punch schedules are properly coordinated
    auto initiator_session = initiator_client->InitiateUpgrade(
        "responder", initiator_addrs);
    ConnectMessage connect_msg = initiator_session->get_connect_message();

    auto responder_session = responder_client->RespondToUpgrade(
        "initiator", responder_addrs, connect_msg);
    SyncMessage sync_msg = responder_session->get_sync_message();

    initiator_session->on_sync_received(sync_msg);

    auto initiator_schedule = initiator_session->get_punch_schedule().value();
    auto responder_schedule = responder_session->get_punch_schedule().value();

    // Both schedules should have similar punch times
    // (within RTT + buffer tolerance)
    int64_t time_diff = std::abs(
        initiator_schedule.punch_time_ns - responder_schedule.punch_time_ns);

    // Time difference should be less than 2 * RTT + 2 * buffer
    int64_t max_diff = 2 * initiator_schedule.rtt_ns +
                       2 * (PUNCH_BUFFER_MS * 1000000LL);

    EXPECT_LT(time_diff, max_diff);
}

class DCUtRPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        client = std::make_unique<DCUtRClient>();
        coordinator = std::make_unique<NATTraversalCoordinator>();
    }

    std::unique_ptr<DCUtRClient> client;
    std::unique_ptr<NATTraversalCoordinator> coordinator;
};

TEST_F(DCUtRPerformanceTest, SessionCreationPerformance) {
    const int num_iterations = 1000;
    std::vector<Address> addrs = {{1, 2, 3, 4}};

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_iterations; i++) {
        std::string peer_id = "peer-" + std::to_string(i);
        auto session = client->InitiateUpgrade(peer_id, addrs);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end - start).count();

    double avg_time_us = static_cast<double>(duration) / num_iterations;

    std::cout << "Average session creation time: " << avg_time_us << " μs" << std::endl;

    // Should be fast (< 100 μs per session)
    EXPECT_LT(avg_time_us, 100.0);
}

TEST_F(DCUtRPerformanceTest, RTTMeasurementPerformance) {
    const int num_iterations = 10000;
    DCUtRCoordinator coord;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_iterations; i++) {
        int64_t t1 = 0;
        int64_t t4 = 1000000000LL;  // 1 second
        int64_t t2 = 200000000LL;
        int64_t t3 = 800000000LL;

        coord.measure_rtt(t1, t4, t2, t3);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
        end - start).count();

    double avg_time_ns = static_cast<double>(duration) / num_iterations;

    std::cout << "Average RTT measurement time: " << avg_time_ns << " ns" << std::endl;

    // Should be very fast (< 1000 ns)
    EXPECT_LT(avg_time_ns, 1000.0);
}

TEST_F(DCUtRPerformanceTest, PunchScheduleCalculationPerformance) {
    const int num_iterations = 10000;
    DCUtRCoordinator coord;
    std::vector<Address> addrs = {{1, 2, 3, 4}};

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_iterations; i++) {
        int64_t t4 = coord.get_current_time_ns();
        int64_t rtt = 100000000LL;  // 100ms

        coord.calculate_initiator_schedule(t4, rtt, addrs);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
        end - start).count();

    double avg_time_ns = static_cast<double>(duration) / num_iterations;

    std::cout << "Average schedule calculation time: " << avg_time_ns << " ns" << std::endl;

    // Should be very fast (< 2000 ns)
    EXPECT_LT(avg_time_ns, 2000.0);
}
