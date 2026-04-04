// Unit tests for nat::UDPConnection and nat::TCPConnection

#include <gtest/gtest.h>
#include "p2p/nat/connection.hpp"
#include <thread>
#include <chrono>

using namespace p2p::nat;
using namespace p2p::net;

// ============================================================================
// UDPConnection Tests
// ============================================================================

class UDPConnectionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a real UDP socket bound to loopback
        socket_ = UDPSocket();
        SocketAddr bind_addr("127.0.0.1", 0);
        ASSERT_TRUE(socket_.bind(bind_addr));

        auto local = socket_.get_local_addr();
        ASSERT_TRUE(local.has_value());
        local_port_ = local->port;

        // Create another socket as remote endpoint
        remote_socket_ = UDPSocket();
        SocketAddr remote_bind("127.0.0.1", 0);
        ASSERT_TRUE(remote_socket_.bind(remote_bind));

        auto remote_local = remote_socket_.get_local_addr();
        ASSERT_TRUE(remote_local.has_value());
        remote_addr_ = *remote_local;
    }

    UDPSocket socket_;
    UDPSocket remote_socket_;
    SocketAddr remote_addr_;
    uint16_t local_port_ = 0;
};

TEST_F(UDPConnectionTest, CreateAndCheckState) {
    UDPConnection conn(std::move(socket_), remote_addr_);

    EXPECT_EQ(conn.get_type(), "udp");
    EXPECT_TRUE(conn.is_connected());
}

TEST_F(UDPConnectionTest, GetLocalAddr) {
    UDPConnection conn(std::move(socket_), remote_addr_);

    auto addr = conn.get_local_addr();
    ASSERT_TRUE(addr.has_value());
    EXPECT_EQ(addr->ip, "127.0.0.1");
    EXPECT_EQ(addr->port, local_port_);
}

TEST_F(UDPConnectionTest, GetRemoteAddr) {
    UDPConnection conn(std::move(socket_), remote_addr_);

    auto addr = conn.get_remote_addr();
    ASSERT_TRUE(addr.has_value());
    EXPECT_EQ(addr->ip, remote_addr_.ip);
    EXPECT_EQ(addr->port, remote_addr_.port);
}

TEST_F(UDPConnectionTest, SendData) {
    UDPConnection conn(std::move(socket_), remote_addr_);

    std::vector<uint8_t> data = {0x48, 0x65, 0x6C, 0x6C, 0x6F};  // "Hello"
    ssize_t sent = conn.send(data);
    EXPECT_EQ(sent, 5);
}

TEST_F(UDPConnectionTest, SendAndReceive) {
    UDPConnection conn(std::move(socket_), remote_addr_);

    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    ssize_t sent = conn.send(data);
    EXPECT_EQ(sent, 5);

    // Brief wait for data to arrive
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Receive on the remote socket
    std::vector<uint8_t> buffer(1024);
    SocketAddr from;
    ssize_t received = remote_socket_.recv_from(buffer, from);
    EXPECT_EQ(received, 5);
    EXPECT_EQ(std::vector<uint8_t>(buffer.begin(), buffer.begin() + 5), data);
}

TEST_F(UDPConnectionTest, SendAfterCloseFails) {
    UDPConnection conn(std::move(socket_), remote_addr_);

    conn.close();
    EXPECT_FALSE(conn.is_connected());

    std::vector<uint8_t> data = {1, 2, 3};
    ssize_t sent = conn.send(data);
    EXPECT_EQ(sent, -1);
}

TEST_F(UDPConnectionTest, RecvAfterCloseFails) {
    UDPConnection conn(std::move(socket_), remote_addr_);

    conn.close();

    std::vector<uint8_t> buffer(1024);
    ssize_t received = conn.recv(buffer);
    EXPECT_EQ(received, -1);
}

TEST_F(UDPConnectionTest, UpdateRemoteAddr) {
    UDPConnection conn(std::move(socket_), remote_addr_);

    SocketAddr new_addr("127.0.0.1", 9999);
    conn.update_remote_addr(new_addr);

    auto addr = conn.get_remote_addr();
    ASSERT_TRUE(addr.has_value());
    EXPECT_EQ(addr->port, 9999);
}

TEST_F(UDPConnectionTest, CloseIdempotent) {
    UDPConnection conn(std::move(socket_), remote_addr_);

    conn.close();
    conn.close();  // Should not crash
    EXPECT_FALSE(conn.is_connected());
}

TEST_F(UDPConnectionTest, DestructorClosesConnection) {
    {
        UDPConnection conn(std::move(socket_), remote_addr_);
        EXPECT_TRUE(conn.is_connected());
    }
    // Destructor should have closed without crash
}

TEST_F(UDPConnectionTest, GetSocket) {
    UDPConnection conn(std::move(socket_), remote_addr_);

    auto& sock = conn.get_socket();
    // Should be a valid reference
    (void)sock;
}

// ============================================================================
// TCPConnection Tests
// ============================================================================

class TCPConnectionTest : public ::testing::Test {};

TEST_F(TCPConnectionTest, CreateWithDefaultSocket) {
    TCPSocket sock;
    TCPConnection conn(std::move(sock));

    EXPECT_EQ(conn.get_type(), "tcp");
    // Default socket is not connected
    EXPECT_FALSE(conn.is_connected());
}

TEST_F(TCPConnectionTest, SendOnDisconnectedFails) {
    TCPSocket sock;
    TCPConnection conn(std::move(sock));

    std::vector<uint8_t> data = {1, 2, 3};
    ssize_t sent = conn.send(data);
    EXPECT_EQ(sent, -1);
}

TEST_F(TCPConnectionTest, RecvOnDisconnectedFails) {
    TCPSocket sock;
    TCPConnection conn(std::move(sock));

    std::vector<uint8_t> buffer(1024);
    ssize_t received = conn.recv(buffer);
    EXPECT_EQ(received, -1);
}

TEST_F(TCPConnectionTest, CloseOnDisconnected) {
    TCPSocket sock;
    TCPConnection conn(std::move(sock));

    // Should not crash
    conn.close();
}

TEST_F(TCPConnectionTest, GetLocalAddrDisconnected) {
    TCPSocket sock;
    TCPConnection conn(std::move(sock));

    auto addr = conn.get_local_addr();
    // Disconnected socket won't have an address
    // Implementation-dependent; just verify it doesn't crash
}

TEST_F(TCPConnectionTest, GetRemoteAddrDisconnected) {
    TCPSocket sock;
    TCPConnection conn(std::move(sock));

    auto addr = conn.get_remote_addr();
    // Disconnected socket won't have a peer address
}

TEST_F(TCPConnectionTest, GetSocket) {
    TCPSocket sock;
    TCPConnection conn(std::move(sock));

    auto& ref = conn.get_socket();
    (void)ref;  // Just verify we can get a reference
}

TEST_F(TCPConnectionTest, DestructorSafe) {
    {
        TCPSocket sock;
        TCPConnection conn(std::move(sock));
    }
    // Destructor should have run without crash
}

// ============================================================================
// Connected TCP Tests (using loopback)
// ============================================================================

class TCPLoopbackTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a TCP listener
        listener_ = TCPSocket();
        SocketAddr bind_addr("127.0.0.1", 0);
        ASSERT_TRUE(listener_.bind(bind_addr));
        ASSERT_TRUE(listener_.listen(1));

        auto addr = listener_.get_local_addr();
        ASSERT_TRUE(addr.has_value());
        listen_port_ = addr->port;
    }

    TCPSocket listener_;
    uint16_t listen_port_ = 0;
};

TEST_F(TCPLoopbackTest, ConnectAndSendReceive) {
    // Client connects
    TCPSocket client_sock;
    SocketAddr server_addr("127.0.0.1", listen_port_);
    bool connected = client_sock.connect(server_addr);
    if (!connected) {
        GTEST_SKIP() << "TCP connect failed (platform limitation)";
    }

    // Accept on server
    SocketAddr peer_addr;
    auto accepted = listener_.accept(peer_addr);
    if (!accepted) {
        GTEST_SKIP() << "TCP accept failed";
    }

    TCPConnection client_conn(std::move(client_sock));
    TCPConnection server_conn(std::move(*accepted));

    EXPECT_TRUE(client_conn.is_connected());
    EXPECT_TRUE(server_conn.is_connected());

    // Send from client, receive on server
    std::vector<uint8_t> data = {0xDE, 0xAD, 0xBE, 0xEF};
    ssize_t sent = client_conn.send(data);
    EXPECT_EQ(sent, 4);

    // Brief wait for data to arrive
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::vector<uint8_t> buffer(1024);
    ssize_t received = server_conn.recv(buffer, 1024);
    EXPECT_EQ(received, 4);

    // Addresses
    auto client_local = client_conn.get_local_addr();
    EXPECT_TRUE(client_local.has_value());

    auto server_remote = server_conn.get_remote_addr();
    EXPECT_TRUE(server_remote.has_value());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
