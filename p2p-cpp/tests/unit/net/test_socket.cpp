#include <gtest/gtest.h>
#include "p2p/net/socket.hpp"
#include <thread>
#include <chrono>

using namespace p2p::net;

class SocketTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// SocketAddr Tests
TEST_F(SocketTest, SocketAddrToString) {
    SocketAddr addr("127.0.0.1", 8080);
    EXPECT_EQ(addr.ToString(), "127.0.0.1:8080");
}

TEST_F(SocketTest, SocketAddrConversion) {
    SocketAddr addr("192.168.1.1", 9000);
    auto sockaddr = addr.ToSockAddr();
    auto converted = SocketAddr::FromSockAddr(sockaddr);

    EXPECT_EQ(converted.ip, addr.ip);
    EXPECT_EQ(converted.port, addr.port);
}

// UDPSocket Tests
TEST_F(SocketTest, UDPSocketCreation) {
    UDPSocket socket;
    EXPECT_TRUE(socket.IsValid());
    EXPECT_GE(socket.GetFd(), 0);
}

TEST_F(SocketTest, UDPSocketBind) {
    UDPSocket socket;
    SocketAddr addr("127.0.0.1", 0);  // Port 0 = auto-assign

    ASSERT_TRUE(socket.Bind(addr));

    auto local_addr = socket.GetLocalAddr();
    ASSERT_TRUE(local_addr.has_value());
    EXPECT_EQ(local_addr->ip, "127.0.0.1");
    EXPECT_GT(local_addr->port, 0);
}

TEST_F(SocketTest, UDPSocketSendRecv) {
    // Create sender and receiver
    UDPSocket sender, receiver;

    SocketAddr recv_addr("127.0.0.1", 0);
    ASSERT_TRUE(receiver.Bind(recv_addr));

    auto local_addr = receiver.GetLocalAddr();
    ASSERT_TRUE(local_addr.has_value());

    // Send data
    std::vector<uint8_t> send_data = {1, 2, 3, 4, 5};
    ssize_t sent = sender.SendTo(send_data, *local_addr);
    EXPECT_EQ(sent, 5);

    // Receive data
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    std::vector<uint8_t> recv_data;
    SocketAddr from;
    ssize_t received = receiver.RecvFrom(recv_data, from);

    EXPECT_EQ(received, 5);
    EXPECT_EQ(recv_data, send_data);
}

TEST_F(SocketTest, UDPSocketMove) {
    UDPSocket socket1;
    int fd1 = socket1.GetFd();

    UDPSocket socket2 = std::move(socket1);

    EXPECT_FALSE(socket1.IsValid());
    EXPECT_TRUE(socket2.IsValid());
    EXPECT_EQ(socket2.GetFd(), fd1);
}

TEST_F(SocketTest, UDPSocketClose) {
    UDPSocket socket;
    EXPECT_TRUE(socket.IsValid());

    socket.Close();
    EXPECT_FALSE(socket.IsValid());
}

// TCPSocket Tests
TEST_F(SocketTest, TCPSocketCreation) {
    TCPSocket socket;
    EXPECT_TRUE(socket.IsValid());
    EXPECT_GE(socket.GetFd(), 0);
}

TEST_F(SocketTest, TCPSocketBindListen) {
    TCPSocket socket;
    SocketAddr addr("127.0.0.1", 0);

    ASSERT_TRUE(socket.Bind(addr));
    ASSERT_TRUE(socket.Listen());

    auto local_addr = socket.GetLocalAddr();
    ASSERT_TRUE(local_addr.has_value());
    EXPECT_EQ(local_addr->ip, "127.0.0.1");
    EXPECT_GT(local_addr->port, 0);
}

TEST_F(SocketTest, TCPSocketConnectAccept) {
    // Create server
    TCPSocket server;
    SocketAddr server_addr("127.0.0.1", 0);
    ASSERT_TRUE(server.Bind(server_addr));
    ASSERT_TRUE(server.Listen());

    auto listen_addr = server.GetLocalAddr();
    ASSERT_TRUE(listen_addr.has_value());

    std::atomic<bool> client_connected{false};
    std::atomic<bool> client_started{false};

    // Create client in separate thread
    std::thread client_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        TCPSocket client;
        client_started = true;
        bool connect_result = client.Connect(*listen_addr);

        if (!connect_result) {
            std::cout << "Connect failed: " << strerror(errno) << std::endl;
        }

        ASSERT_TRUE(connect_result);

        // Wait for connection to establish (up to 2 seconds)
        for (int i = 0; i < 200; i++) {
            if (client.IsConnected()) {
                client_connected = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        EXPECT_TRUE(client.IsConnected());

        // Keep connection alive
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    });

    // Wait for client to start connecting
    while (!client_started) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Give client time to initiate connection
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Accept connection with retry
    SocketAddr peer_addr;
    std::unique_ptr<TCPSocket> client_socket;

    for (int i = 0; i < 20; i++) {
        client_socket = server.Accept(peer_addr);
        if (client_socket != nullptr) {
            std::cout << "Accepted connection on attempt " << (i+1) << std::endl;
            break;
        }
        if (i == 19) {
            std::cout << "Accept failed after 20 attempts, errno: " << strerror(errno) << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    client_thread.join();

    ASSERT_NE(client_socket, nullptr);
    EXPECT_TRUE(client_socket->IsValid());
    EXPECT_EQ(peer_addr.ip, "127.0.0.1");
    EXPECT_TRUE(client_connected);
}

TEST_F(SocketTest, TCPSocketSendRecv) {
    // Create server
    TCPSocket server;
    SocketAddr server_addr("127.0.0.1", 0);
    ASSERT_TRUE(server.Bind(server_addr));
    ASSERT_TRUE(server.Listen());

    auto listen_addr = server.GetLocalAddr();
    ASSERT_TRUE(listen_addr.has_value());

    std::vector<uint8_t> received_data;
    std::atomic<bool> data_received{false};

    // Server thread
    std::thread server_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        SocketAddr peer_addr;
        std::unique_ptr<TCPSocket> client;

        // Accept with retry
        for (int i = 0; i < 20; i++) {
            client = server.Accept(peer_addr);
            if (client != nullptr) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        ASSERT_NE(client, nullptr);

        // Wait for data with timeout
        for (int i = 0; i < 100; i++) {
            ssize_t n = client->Recv(received_data);
            if (n > 0) {
                data_received = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // Client thread
    std::thread client_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        TCPSocket client;
        ASSERT_TRUE(client.Connect(*listen_addr));

        // Wait for connection (up to 2 seconds)
        for (int i = 0; i < 200; i++) {
            if (client.IsConnected()) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        ASSERT_TRUE(client.IsConnected());

        // Give server time to accept
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Send data
        std::vector<uint8_t> send_data = {1, 2, 3, 4, 5, 6, 7, 8};
        ssize_t sent = client.Send(send_data);
        EXPECT_EQ(sent, 8);

        // Keep connection alive
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    });

    server_thread.join();
    client_thread.join();

    EXPECT_TRUE(data_received);
    EXPECT_EQ(received_data.size(), 8);
    EXPECT_EQ(received_data, std::vector<uint8_t>({1, 2, 3, 4, 5, 6, 7, 8}));
}

TEST_F(SocketTest, TCPSocketMove) {
    TCPSocket socket1;
    int fd1 = socket1.GetFd();

    TCPSocket socket2 = std::move(socket1);

    EXPECT_FALSE(socket1.IsValid());
    EXPECT_TRUE(socket2.IsValid());
    EXPECT_EQ(socket2.GetFd(), fd1);
}

TEST_F(SocketTest, TCPSocketClose) {
    TCPSocket socket;
    EXPECT_TRUE(socket.IsValid());

    socket.Close();
    EXPECT_FALSE(socket.IsValid());
}

// SocketManager Tests
TEST_F(SocketTest, SocketManagerCreation) {
    SocketManager manager;
    EXPECT_FALSE(manager.IsRunning());
}

TEST_F(SocketTest, SocketManagerRegisterUnregister) {
    SocketManager manager;
    UDPSocket socket;

    bool callback_called = false;
    auto callback = [&](int fd, SocketEvent event) {
        callback_called = true;
    };

    EXPECT_TRUE(manager.Register(socket.GetFd(), callback, EPOLLIN));
    EXPECT_TRUE(manager.Unregister(socket.GetFd()));
}

TEST_F(SocketTest, SocketManagerPollReadable) {
    SocketManager manager;

    // Create UDP sockets
    UDPSocket sender, receiver;
    SocketAddr recv_addr("127.0.0.1", 0);
    ASSERT_TRUE(receiver.Bind(recv_addr));

    auto local_addr = receiver.GetLocalAddr();
    ASSERT_TRUE(local_addr.has_value());

    // Register receiver
    bool readable = false;
    auto callback = [&](int fd, SocketEvent event) {
        if (event == SocketEvent::READABLE) {
            readable = true;
            manager.Stop();
        }
    };

    ASSERT_TRUE(manager.Register(receiver.GetFd(), callback, EPOLLIN));

    // Send data in separate thread
    std::thread sender_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::vector<uint8_t> data = {1, 2, 3};
        sender.SendTo(data, *local_addr);
    });

    // Poll for events
    manager.Poll(1000);  // 1 second timeout

    sender_thread.join();

    EXPECT_TRUE(readable);
}

TEST_F(SocketTest, SocketManagerStop) {
    SocketManager manager;

    std::atomic<int> poll_count{0};

    std::thread poll_thread([&]() {
        // Use finite timeout so we can check running flag
        while (manager.IsRunning() || poll_count == 0) {
            manager.Poll(100);  // 100ms timeout
            poll_count++;
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    manager.Stop();

    poll_thread.join();

    EXPECT_FALSE(manager.IsRunning());
    EXPECT_GT(poll_count, 0);
}

// Performance Tests
TEST_F(SocketTest, UDPSocketPerformance) {
    UDPSocket sender, receiver;
    SocketAddr recv_addr("127.0.0.1", 0);
    ASSERT_TRUE(receiver.Bind(recv_addr));

    auto local_addr = receiver.GetLocalAddr();
    ASSERT_TRUE(local_addr.has_value());

    const int num_packets = 10000;
    std::vector<uint8_t> data(1024);  // 1KB packets

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_packets; i++) {
        sender.SendTo(data, *local_addr);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double avg_time_us = static_cast<double>(duration.count()) / num_packets;

    std::cout << "Average UDP send time: " << avg_time_us << " μs" << std::endl;

    // Should be fast (< 100 μs per packet)
    EXPECT_LT(avg_time_us, 100.0);
}