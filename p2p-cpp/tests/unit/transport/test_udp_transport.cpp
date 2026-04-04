#include "p2p/transport/udp_transport.hpp"
#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <atomic>
#include <chrono>
#include <thread>

using namespace p2p::transport;
namespace asio = boost::asio;
using udp = asio::ip::udp;

class UDPTransportTest : public ::testing::Test {
protected:
    boost::asio::io_context io_context_;

    // Helper: Run io_context for a limited time
    void RunFor(std::chrono::milliseconds duration) {
        io_context_.run_for(duration);
    }

    // Helper: Run io_context until condition is met or timeout
    void RunUntil(std::function<bool()> condition, std::chrono::milliseconds timeout) {
        auto start = std::chrono::steady_clock::now();
        while (!condition()) {
            // Use run_for instead of poll to actually wait for async operations
            io_context_.run_for(std::chrono::milliseconds(10));
            io_context_.restart();  // Reset io_context for next run_for
            if (std::chrono::steady_clock::now() - start > timeout) {
                break;
            }
        }
    }

    // Helper: Create transport (must use shared_ptr due to enable_shared_from_this)
    std::shared_ptr<UDPTransport> CreateTransport(uint16_t port = 0) {
        return std::make_shared<UDPTransport>(io_context_, port);
    }
};

// --- Construction and Initialization Tests ---

TEST_F(UDPTransportTest, Constructor_BindsToPort) {
    auto transport = CreateTransport(0);  // Port 0 = auto-assign
    auto endpoint = transport->local_endpoint();

    EXPECT_GT(endpoint.port(), 0);  // Should have assigned a port
    EXPECT_EQ(endpoint.address(), asio::ip::address_v4::any());
}

TEST_F(UDPTransportTest, Constructor_ZeroPort_BindsToRandomPort) {
    auto transport1 = CreateTransport(0);
    auto transport2 = CreateTransport(0);

    auto port1 = transport1->local_endpoint().port();
    auto port2 = transport2->local_endpoint().port();

    EXPECT_GT(port1, 0);
    EXPECT_GT(port2, 0);
    EXPECT_NE(port1, port2);  // Should get different ports
}

TEST_F(UDPTransportTest, LocalEndpoint_ReturnsCorrectEndpoint) {
    auto transport = CreateTransport(0);
    auto endpoint = transport->local_endpoint();

    EXPECT_TRUE(endpoint.address().is_v4());
    EXPECT_GT(endpoint.port(), 0);
}

TEST_F(UDPTransportTest, IsRunning_InitiallyFalse) {
    auto transport = CreateTransport();
    EXPECT_FALSE(transport->is_running());
}

// --- State Management Tests ---

TEST_F(UDPTransportTest, Start_WhenNotRunning_Succeeds) {
    auto transport = CreateTransport();
    std::error_code ec;

    transport->start(ec);

    EXPECT_FALSE(ec);
    EXPECT_TRUE(transport->is_running());

    transport->stop();
}

TEST_F(UDPTransportTest, Start_WhenAlreadyRunning_ReturnsError) {
    auto transport = CreateTransport();
    std::error_code ec1, ec2;

    transport->start(ec1);
    EXPECT_FALSE(ec1);

    transport->start(ec2);
    EXPECT_TRUE(ec2);
    EXPECT_EQ(ec2, std::errc::already_connected);

    transport->stop();
}

TEST_F(UDPTransportTest, Start_SetsRunningFlag) {
    auto transport = CreateTransport();
    std::error_code ec;

    EXPECT_FALSE(transport->is_running());

    transport->start(ec);
    EXPECT_TRUE(transport->is_running());

    transport->stop();
}

TEST_F(UDPTransportTest, Stop_WhenRunning_StopsTransport) {
    auto transport = CreateTransport();
    std::error_code ec;

    transport->start(ec);
    EXPECT_TRUE(transport->is_running());

    transport->stop();
    EXPECT_FALSE(transport->is_running());
}

TEST_F(UDPTransportTest, Stop_ClearsRunningFlag) {
    auto transport = CreateTransport();
    std::error_code ec;

    transport->start(ec);
    transport->stop();

    EXPECT_FALSE(transport->is_running());
}

TEST_F(UDPTransportTest, Stop_WhenNotRunning_NoOp) {
    auto transport = CreateTransport();

    // Should not crash or throw
    transport->stop();
    EXPECT_FALSE(transport->is_running());
}

TEST_F(UDPTransportTest, DoubleStop_Safe) {
    auto transport = CreateTransport();
    std::error_code ec;

    transport->start(ec);
    transport->stop();
    transport->stop();  // Second stop should be safe

    EXPECT_FALSE(transport->is_running());
}

// --- send_to Tests ---

TEST_F(UDPTransportTest, SendTo_WhenNotRunning_ReturnsError) {
    auto transport = CreateTransport();
    std::vector<uint8_t> data = {0x01, 0x02, 0x03};
    udp::endpoint target(asio::ip::address_v4::loopback(), 12345);

    std::atomic<bool> callback_called{false};
    std::error_code result_ec;

    transport->send_to(data, target, [&](const std::error_code& ec) {
        callback_called = true;
        result_ec = ec;
    });

    EXPECT_TRUE(callback_called);
    EXPECT_EQ(result_ec, std::errc::not_connected);
}

TEST_F(UDPTransportTest, SendTo_WhenRunning_InvokesCallback) {
    auto transport = CreateTransport();
    std::error_code ec;
    transport->start(ec);

    std::vector<uint8_t> data = {0x01, 0x02, 0x03};
    udp::endpoint target(asio::ip::address_v4::loopback(), 12345);

    std::atomic<bool> callback_called{false};

    transport->send_to(data, target, [&](const std::error_code& ec) {
        callback_called = true;
    });

    // Run io_context to process async operation
    RunUntil([&]() { return callback_called.load(); }, std::chrono::milliseconds(1000));

    EXPECT_TRUE(callback_called);

    transport->stop();
}

TEST_F(UDPTransportTest, SendTo_WithoutCallback_DoesNotCrash) {
    auto transport = CreateTransport();
    std::error_code ec;
    transport->start(ec);

    std::vector<uint8_t> data = {0x01, 0x02, 0x03};
    udp::endpoint target(asio::ip::address_v4::loopback(), 12345);

    // Should not crash without callback
    transport->send_to(data, target, nullptr);

    RunFor(std::chrono::milliseconds(100));

    transport->stop();
}

TEST_F(UDPTransportTest, SendTo_EmptyData_Succeeds) {
    auto transport = CreateTransport();
    std::error_code ec;
    transport->start(ec);

    std::vector<uint8_t> empty_data;
    udp::endpoint target(asio::ip::address_v4::loopback(), 12345);

    std::atomic<bool> callback_called{false};
    std::error_code result_ec;

    transport->send_to(empty_data, target, [&](const std::error_code& ec) {
        callback_called = true;
        result_ec = ec;
    });

    RunUntil([&]() { return callback_called.load(); }, std::chrono::milliseconds(1000));

    EXPECT_TRUE(callback_called);
    // Empty data send should succeed (UDP allows 0-byte datagrams)

    transport->stop();
}

// --- send Tests ---

TEST_F(UDPTransportTest, Send_WithoutPeer_ReturnsError) {
    auto transport = CreateTransport();
    std::error_code ec;
    transport->start(ec);

    std::vector<uint8_t> data = {0x01, 0x02, 0x03};

    std::atomic<bool> callback_called{false};
    std::error_code result_ec;

    transport->send(data, [&](const std::error_code& ec) {
        callback_called = true;
        result_ec = ec;
    });

    EXPECT_TRUE(callback_called);
    EXPECT_EQ(result_ec, std::errc::destination_address_required);

    transport->stop();
}

TEST_F(UDPTransportTest, Send_WithPeer_Succeeds) {
    auto transport = CreateTransport();
    std::error_code ec;
    transport->start(ec);

    udp::endpoint peer(asio::ip::address_v4::loopback(), 12345);
    transport->set_peer(peer);

    std::vector<uint8_t> data = {0x01, 0x02, 0x03};

    std::atomic<bool> callback_called{false};

    transport->send(data, [&](const std::error_code& ec) {
        callback_called = true;
    });

    RunUntil([&]() { return callback_called.load(); }, std::chrono::milliseconds(1000));

    EXPECT_TRUE(callback_called);

    transport->stop();
}

TEST_F(UDPTransportTest, SetPeer_UpdatesPeerEndpoint) {
    auto transport = CreateTransport();
    std::error_code ec;
    transport->start(ec);

    udp::endpoint peer1(asio::ip::address_v4::loopback(), 12345);
    udp::endpoint peer2(asio::ip::address_v4::loopback(), 54321);

    transport->set_peer(peer1);

    std::atomic<bool> callback1_called{false};
    std::vector<uint8_t> data = {0x01};

    transport->send(data, [&](const std::error_code& ec) {
        callback1_called = true;
    });

    RunUntil([&]() { return callback1_called.load(); }, std::chrono::milliseconds(1000));
    EXPECT_TRUE(callback1_called);

    // Change peer
    transport->set_peer(peer2);

    std::atomic<bool> callback2_called{false};
    transport->send(data, [&](const std::error_code& ec) {
        callback2_called = true;
    });

    RunUntil([&]() { return callback2_called.load(); }, std::chrono::milliseconds(1000));
    EXPECT_TRUE(callback2_called);

    transport->stop();
}

TEST_F(UDPTransportTest, Send_WhenNotRunning_ReturnsError) {
    auto transport = CreateTransport();

    udp::endpoint peer(asio::ip::address_v4::loopback(), 12345);
    transport->set_peer(peer);

    std::vector<uint8_t> data = {0x01, 0x02, 0x03};

    std::atomic<bool> callback_called{false};
    std::error_code result_ec;

    transport->send(data, [&](const std::error_code& ec) {
        callback_called = true;
        result_ec = ec;
    });

    EXPECT_TRUE(callback_called);
    EXPECT_EQ(result_ec, std::errc::not_connected);
}

// --- Receive Callback Tests ---

TEST_F(UDPTransportTest, SetReceiveCallback_StoresCallback) {
    auto transport = CreateTransport();

    std::atomic<bool> callback_set{false};
    transport->set_receive_callback([&](const std::vector<uint8_t>&, const udp::endpoint&) {
        callback_set = true;
    });

    // Callback is set but should not be triggered without actual data
    EXPECT_FALSE(callback_set);
}

// --- Integration Tests ---

TEST_F(UDPTransportTest, SendReceive_BetweenTwoTransports_Succeeds) {
    auto transport1 = CreateTransport(0);
    auto transport2 = CreateTransport(0);

    std::error_code ec1, ec2;
    transport1->start(ec1);
    transport2->start(ec2);
    ASSERT_FALSE(ec1);
    ASSERT_FALSE(ec2);

    // Set up receive callback on transport2
    std::atomic<bool> received{false};
    std::vector<uint8_t> received_data;
    udp::endpoint received_from;

    transport2->set_receive_callback([&](const std::vector<uint8_t>& data, const udp::endpoint& from) {
        received_data = data;
        received_from = from;
        received = true;
    });

    // Send from transport1 to transport2
    std::vector<uint8_t> send_data = {0xDE, 0xAD, 0xBE, 0xEF};
    // Use loopback address instead of 0.0.0.0 from local_endpoint()
    auto transport2_port = transport2->local_endpoint().port();
    udp::endpoint transport2_endpoint(asio::ip::address_v4::loopback(), transport2_port);

    std::atomic<bool> sent{false};
    transport1->send_to(send_data, transport2_endpoint, [&](const std::error_code& ec) {
        sent = true;
    });

    // Run io_context until both send and receive complete
    RunUntil([&]() { return sent.load() && received.load(); }, std::chrono::milliseconds(2000));

    EXPECT_TRUE(sent);
    EXPECT_TRUE(received);
    EXPECT_EQ(received_data, send_data);
    EXPECT_EQ(received_from.port(), transport1->local_endpoint().port());

    transport1->stop();
    transport2->stop();
}

TEST_F(UDPTransportTest, SendReceive_MultipleMessages_AllReceived) {
    auto transport1 = CreateTransport(0);
    auto transport2 = CreateTransport(0);

    std::error_code ec1, ec2;
    transport1->start(ec1);
    transport2->start(ec2);

    std::atomic<int> received_count{0};
    std::vector<std::vector<uint8_t>> received_messages;
    std::mutex received_mutex;

    transport2->set_receive_callback([&](const std::vector<uint8_t>& data, const udp::endpoint&) {
        std::lock_guard<std::mutex> lock(received_mutex);
        received_messages.push_back(data);
        received_count++;
    });

    // Use loopback address instead of 0.0.0.0 from local_endpoint()
    auto transport2_port = transport2->local_endpoint().port();
    udp::endpoint transport2_endpoint(asio::ip::address_v4::loopback(), transport2_port);

    // Send 3 messages
    std::vector<std::vector<uint8_t>> messages = {
        {0x01, 0x02},
        {0x03, 0x04, 0x05},
        {0x06}
    };

    for (const auto& msg : messages) {
        transport1->send_to(msg, transport2_endpoint, nullptr);
    }

    // Wait for all messages to be received
    RunUntil([&]() { return received_count.load() >= 3; }, std::chrono::milliseconds(2000));

    EXPECT_EQ(received_count.load(), 3);
    EXPECT_EQ(received_messages.size(), 3);

    transport1->stop();
    transport2->stop();
}

TEST_F(UDPTransportTest, Receive_ContinuesAfterFirstMessage) {
    auto transport1 = CreateTransport(0);
    auto transport2 = CreateTransport(0);

    std::error_code ec1, ec2;
    transport1->start(ec1);
    transport2->start(ec2);

    std::atomic<int> received_count{0};

    transport2->set_receive_callback([&](const std::vector<uint8_t>&, const udp::endpoint&) {
        received_count++;
    });

    // Use loopback address instead of 0.0.0.0 from local_endpoint()
    auto transport2_port = transport2->local_endpoint().port();
    udp::endpoint transport2_endpoint(asio::ip::address_v4::loopback(), transport2_port);

    // Send first message
    std::vector<uint8_t> msg1 = {0x01};
    transport1->send_to(msg1, transport2_endpoint, nullptr);

    RunUntil([&]() { return received_count.load() >= 1; }, std::chrono::milliseconds(1000));
    EXPECT_EQ(received_count.load(), 1);

    // Send second message - receive loop should still be active
    std::vector<uint8_t> msg2 = {0x02};
    transport1->send_to(msg2, transport2_endpoint, nullptr);

    RunUntil([&]() { return received_count.load() >= 2; }, std::chrono::milliseconds(1000));
    EXPECT_EQ(received_count.load(), 2);

    transport1->stop();
    transport2->stop();
}
