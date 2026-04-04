#include <gtest/gtest.h>
#include "p2p/core/connection.hpp"
#include "p2p/core/tcp_connection.hpp"
#include <boost/asio.hpp>
#include <thread>
#include <chrono>

using namespace p2p;

class TcpConnectionTest : public ::testing::Test {
protected:
    void SetUp() override {
        io_context_ = std::make_shared<boost::asio::io_context>();
        work_guard_ = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
            io_context_->get_executor()
        );
        io_thread_ = std::thread([this]() { io_context_->run(); });
    }

    void TearDown() override {
        work_guard_.reset();
        io_context_->stop();
        if (io_thread_.joinable()) {
            io_thread_.join();
        }
    }

    std::shared_ptr<boost::asio::io_context> io_context_;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard_;
    std::thread io_thread_;
};

// Test connection creation
TEST_F(TcpConnectionTest, CreateConnection) {
    auto conn = TcpConnection::Create(*io_context_, PeerId("test-peer"));
    ASSERT_NE(conn, nullptr);
    EXPECT_EQ(conn->GetState(), ConnectionState::DISCONNECTED);
}

// Test connection state transitions
TEST_F(TcpConnectionTest, StateTransitions) {
    auto conn = TcpConnection::Create(*io_context_, PeerId("test-peer"));

    // Initial state
    EXPECT_EQ(conn->GetState(), ConnectionState::DISCONNECTED);

    // After close (should remain disconnected)
    auto status = conn->Close();
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(conn->GetState(), ConnectionState::DISCONNECTED);
}

// Test send on disconnected connection
TEST_F(TcpConnectionTest, SendOnDisconnectedConnection) {
    auto conn = TcpConnection::Create(*io_context_, PeerId("test-peer"));

    std::vector<uint8_t> data = {1, 2, 3, 4};
    auto status = conn->Send(std::span<const uint8_t>(data));

    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.code(), StatusCode::ERROR_CONNECTION_FAILED);
}

// Test peer ID
TEST_F(TcpConnectionTest, GetPeerId) {
    PeerId peer_id("test-peer-123");
    auto conn = TcpConnection::Create(*io_context_, peer_id);

    EXPECT_EQ(conn->GetPeerId(), peer_id);
}

// Test connection ID uniqueness
TEST_F(TcpConnectionTest, UniqueConnectionIds) {
    auto conn1 = TcpConnection::Create(*io_context_, PeerId("peer1"));
    auto conn2 = TcpConnection::Create(*io_context_, PeerId("peer2"));

    EXPECT_NE(conn1->GetId(), conn2->GetId());
}

}  // namespace
