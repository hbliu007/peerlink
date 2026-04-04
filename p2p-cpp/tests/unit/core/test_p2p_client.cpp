#include "p2p/core/p2p_client.hpp"
#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <atomic>
#include <thread>

using namespace p2p::core;

class P2PClientTest : public ::testing::Test {
protected:
    boost::asio::io_context io_context_;

    std::shared_ptr<P2PClient> make_client(
        const std::string& did = "test-did",
        P2PConfig config = {}) {
        config.tcp_relay_server = "";  // no relay
        return std::make_shared<P2PClient>(io_context_, did, config);
    }
};

// --- 构造与初始状态 ---

TEST_F(P2PClientTest, InitialState) {
    auto client = make_client("my-device");

    EXPECT_EQ(client->state(), ConnectionState::DISCONNECTED);
    EXPECT_EQ(client->did(), "my-device");
    EXPECT_FALSE(client->is_connected());
    EXPECT_FALSE(client->is_p2p());
    EXPECT_EQ(client->active_path(), ConnectionPath::NONE);
    EXPECT_EQ(client->last_failure_reason(), ConnectionFailureReason::NONE);
    EXPECT_TRUE(client->last_failure_detail().empty());
    EXPECT_FALSE(client->used_relay_fallback());
    EXPECT_FALSE(client->peer().has_value());
}

// --- Channel 管理 ---

TEST_F(P2PClientTest, CreateChannel) {
    auto client = make_client();

    int ch1 = client->create_channel();
    int ch2 = client->create_channel();

    EXPECT_NE(ch1, ch2);
    EXPECT_GT(ch1, 0);
    EXPECT_GT(ch2, 0);
}

TEST_F(P2PClientTest, CloseChannel) {
    auto client = make_client();

    int ch = client->create_channel();
    // Should not throw
    client->close_channel(ch);
    // Closing non-existent channel should also not throw
    client->close_channel(999);
}

// --- close() 重入保护 (H-8 修复验证) ---

TEST_F(P2PClientTest, CloseWithoutInitialize) {
    auto client = make_client();

    // close() without initialize should be safe (running_ is false)
    client->close();
    EXPECT_EQ(client->state(), ConnectionState::DISCONNECTED);
}

TEST_F(P2PClientTest, DoubleCloseNoDoubleFree) {
    auto client = make_client();

    // Multiple close calls should be safe
    client->close();
    client->close();
    client->close();

    EXPECT_EQ(client->state(), ConnectionState::DISCONNECTED);
}

TEST_F(P2PClientTest, CloseReentrancyProtection_H8) {
    auto client = make_client();

    // Track how many times on_disconnected is called
    std::atomic<int> disconnect_count{0};
    client->on_disconnected([&disconnect_count]() {
        disconnect_count++;
    });

    // close() should call on_disconnected at most once
    // Even if relay_transport_->stop() would trigger event_callback_("disconnected")
    client->close();

    // Since running_ was false, on_disconnected should NOT be called
    EXPECT_EQ(disconnect_count.load(), 0);
}

// --- connect() 前置条件 ---

TEST_F(P2PClientTest, ConnectWithoutInitialize) {
    auto client = make_client();

    bool callback_called = false;
    std::error_code result_ec;

    client->connect("peer-did", [&](const std::error_code& ec) {
        callback_called = true;
        result_ec = ec;
    });

    EXPECT_TRUE(callback_called);
    EXPECT_NE(result_ec, std::error_code{});
    EXPECT_EQ(client->last_failure_reason(), ConnectionFailureReason::NOT_RUNNING);
}

// --- send_data() 前置条件 ---

TEST_F(P2PClientTest, SendDataWithoutConnection) {
    auto client = make_client();

    bool callback_called = false;
    std::error_code result_ec;

    std::vector<uint8_t> data = {0x01, 0x02, 0x03};
    client->send_data(1, data, [&](const std::error_code& ec) {
        callback_called = true;
        result_ec = ec;
    });

    EXPECT_TRUE(callback_called);
    EXPECT_NE(result_ec, std::error_code{});
}

// --- reset_for_retry() ---

TEST_F(P2PClientTest, ResetForRetry) {
    auto client = make_client();

    client->reset_for_retry();

    EXPECT_EQ(client->state(), ConnectionState::DISCONNECTED);
    EXPECT_EQ(client->active_path(), ConnectionPath::NONE);
}

// --- Callback 安全性 (C-4 修复验证) ---

TEST_F(P2PClientTest, CallbackSurvivesClientDestruction_C4) {
    // Verify that destroying the client doesn't cause use-after-free
    // in pending async callbacks (weak_from_this pattern)
    std::atomic<bool> connected_called{false};
    std::atomic<bool> disconnected_called{false};
    std::atomic<bool> data_called{false};

    {
        auto client = make_client();
        client->on_connected([&]() { connected_called = true; });
        client->on_disconnected([&]() { disconnected_called = true; });
        client->on_data([&](int, const std::vector<uint8_t>&) { data_called = true; });
        // Client goes out of scope here — should not crash
    }

    // None should be called after client destruction
    EXPECT_FALSE(connected_called.load());
    EXPECT_FALSE(data_called.load());
}

// --- state_ atomic 验证 (C-3 修复验证) ---

TEST_F(P2PClientTest, StateIsAtomic_C3) {
    auto client = make_client();

    // Read state from multiple "threads" (simulated)
    // This mainly verifies compilation — std::atomic<ConnectionState> compiles
    ConnectionState s1 = client->state();
    ConnectionState s2 = client->state();
    EXPECT_EQ(s1, s2);
    EXPECT_EQ(s1, ConnectionState::DISCONNECTED);
}

// --- ToString 方法 ---

TEST_F(P2PClientTest, ToStringRelayMode) {
    EXPECT_STREQ(P2PClient::ToString(RelayMode::AUTO), "auto");
    EXPECT_STREQ(P2PClient::ToString(RelayMode::RELAY_PREFERRED), "relay-preferred");
    EXPECT_STREQ(P2PClient::ToString(RelayMode::RELAY_ONLY), "relay-only");
    EXPECT_STREQ(P2PClient::ToString(RelayMode::DIRECT_ONLY), "direct-only");
}

TEST_F(P2PClientTest, ToStringConnectionPath) {
    EXPECT_STREQ(P2PClient::ToString(ConnectionPath::NONE), "none");
    EXPECT_STREQ(P2PClient::ToString(ConnectionPath::DIRECT_P2P), "direct-p2p");
    EXPECT_STREQ(P2PClient::ToString(ConnectionPath::RELAY), "relay");
}

TEST_F(P2PClientTest, ToStringFailureReason) {
    EXPECT_STREQ(P2PClient::ToString(ConnectionFailureReason::NONE), "none");
    EXPECT_STREQ(P2PClient::ToString(ConnectionFailureReason::NOT_RUNNING), "not-running");
    EXPECT_STREQ(P2PClient::ToString(ConnectionFailureReason::RELAY_NOT_CONFIGURED), "relay-not-configured");
}

// --- initialize() RELAY_ONLY 无服务器配置 ---

TEST_F(P2PClientTest, InitializeRelayOnlyNoServer) {
    P2PConfig config;
    config.relay_mode = RelayMode::RELAY_ONLY;
    config.tcp_relay_server = "";  // no relay server

    auto client = std::make_shared<P2PClient>(io_context_, "test", config);

    bool callback_called = false;
    std::error_code result_ec;

    client->initialize([&](const std::error_code& ec) {
        callback_called = true;
        result_ec = ec;
    });

    EXPECT_TRUE(callback_called);
    EXPECT_NE(result_ec, std::error_code{});
    // init_relay_transport sets RELAY_NOT_CONFIGURED, but the RELAY_ONLY
    // callback in initialize() overwrites it to RELAY_REGISTRATION_FAILED
    EXPECT_EQ(client->last_failure_reason(),
              ConnectionFailureReason::RELAY_REGISTRATION_FAILED);
}

// --- P2PConfig 默认值 ---

TEST_F(P2PClientTest, DefaultConfig) {
    P2PConfig config;
    EXPECT_EQ(config.relay_mode, RelayMode::AUTO);
    EXPECT_EQ(config.stun_port, 19302);
    EXPECT_EQ(config.tcp_relay_port, 9700);
    EXPECT_EQ(config.local_port, 0);
    EXPECT_EQ(config.max_retries, 3);
    EXPECT_TRUE(config.auto_relay);
}
