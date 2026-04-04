#include <gtest/gtest.h>
#include "p2p/nat/puncher.hpp"
#include "p2p/net/socket.hpp"
#include <thread>
#include <chrono>

using namespace p2p::nat;
using namespace p2p::protocol;

// Mock Connection for testing
class MockConnection : public Connection {
public:
    explicit MockConnection(const std::string& type) : type_(type), connected_(true) {}

    std::string get_type() const override { return type_; }
    bool is_connected() const override { return connected_; }
    ssize_t send(const std::vector<uint8_t>& data) override { return static_cast<ssize_t>(data.size()); }
    ssize_t recv(std::vector<uint8_t>& buffer, size_t max_size = 65536) override { return 0; }
    void close() override { connected_ = false; }
    std::optional<p2p::net::SocketAddr> get_local_addr() const override { return std::nullopt; }
    std::optional<p2p::net::SocketAddr> get_remote_addr() const override { return std::nullopt; }

private:
    std::string type_;
    bool connected_;
};

class UDPPuncherTest : public ::testing::Test {
protected:
    UDPPuncher puncher;
    std::vector<Address> target_addrs = {{1, 2, 3, 4}};
};

TEST_F(UDPPuncherTest, get_transport_type) {
    EXPECT_EQ(puncher.get_transport_type(), "udp");
}

TEST_F(UDPPuncherTest, PunchWithoutRealPeerFails) {
    int64_t punch_time = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count() + 10000000LL;  // 10ms from now

    auto future = puncher.punch(target_addrs, punch_time);
    PunchResult result = future.get();

    // Synthetic multiaddr bytes are not a reachable peer; hole punch cannot succeed in unit tests.
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error.empty());
}

TEST_F(UDPPuncherTest, PunchNoAddresses) {
    int64_t punch_time = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count() + 10000000LL;

    std::vector<Address> empty_addrs;
    auto future = puncher.punch(empty_addrs, punch_time);
    PunchResult result = future.get();

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error.empty());
}

class TCPPuncherTest : public ::testing::Test {
protected:
    TCPPuncher puncher;
    std::vector<Address> target_addrs = {{5, 6, 7, 8}};
};

TEST_F(TCPPuncherTest, get_transport_type) {
    EXPECT_EQ(puncher.get_transport_type(), "tcp");
}

TEST_F(TCPPuncherTest, PunchWithoutRealPeerFails) {
    int64_t punch_time = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count() + 10000000LL;

    auto future = puncher.punch(target_addrs, punch_time);
    PunchResult result = future.get();

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error.empty());
}

class NATTraversalCoordinatorTest : public ::testing::Test {
protected:
    NATTraversalCoordinator coordinator;
    std::vector<Address> target_addrs = {{1, 2, 3, 4}};
};

TEST_F(NATTraversalCoordinatorTest, ExecuteCoordinatedPunch) {
    PunchSchedule schedule;
    schedule.punch_time_ns = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count() + 10000000LL;  // 10ms from now
    schedule.target_addrs = target_addrs;
    schedule.rtt_ns = 50000000LL;

    bool callback_called = false;
    PunchResult callback_result;

    coordinator.execute_coordinated_punch(schedule, [&](const PunchResult& result) {
        callback_called = true;
        callback_result = result;
    });

    EXPECT_TRUE(callback_called);
    // No real peer on loopback for synthetic addresses; both UDP and TCP attempts fail.
    EXPECT_FALSE(callback_result.success);
    EXPECT_FALSE(callback_result.error.empty());
}

TEST_F(NATTraversalCoordinatorTest, ExecuteWithRelayFallback_UsesRelayWhenPunchFails) {
    PunchSchedule schedule;
    schedule.punch_time_ns = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count() + 10000000LL;
    schedule.target_addrs = target_addrs;
    schedule.rtt_ns = 50000000LL;

    auto relay_conn = std::make_shared<MockConnection>("relay");

    bool callback_called = false;
    PunchResult callback_result;

    coordinator.execute_with_relay_fallback(schedule, relay_conn,
        [&](const PunchResult& result) {
            callback_called = true;
            callback_result = result;
        });

    EXPECT_TRUE(callback_called);
    EXPECT_TRUE(callback_result.success);
    EXPECT_EQ(callback_result.transport_type, "relay");
    EXPECT_EQ(callback_result.connection, relay_conn);
}

TEST_F(NATTraversalCoordinatorTest, ExecuteWithRelayFallback_DirectFail) {
    PunchSchedule schedule;
    schedule.punch_time_ns = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count() + 10000000LL;
    // Empty addresses will cause punch to fail
    schedule.target_addrs = {};
    schedule.rtt_ns = 50000000LL;

    auto relay_conn = std::make_shared<MockConnection>("relay");

    bool callback_called = false;
    PunchResult callback_result;

    coordinator.execute_with_relay_fallback(schedule, relay_conn,
        [&](const PunchResult& result) {
            callback_called = true;
            callback_result = result;
        });

    EXPECT_TRUE(callback_called);
    EXPECT_TRUE(callback_result.success);
    // Should fallback to relay
    EXPECT_EQ(callback_result.transport_type, "relay");
    EXPECT_EQ(callback_result.connection, relay_conn);
}

class MockConnectionTest : public ::testing::Test {};

TEST_F(MockConnectionTest, CreateConnection) {
    MockConnection conn("udp");
    EXPECT_EQ(conn.get_type(), "udp");
}

TEST_F(MockConnectionTest, CreateTCPConnection) {
    MockConnection conn("tcp");
    EXPECT_EQ(conn.get_type(), "tcp");
}

TEST_F(MockConnectionTest, CreateRelayConnection) {
    MockConnection conn("relay");
    EXPECT_EQ(conn.get_type(), "relay");
}
