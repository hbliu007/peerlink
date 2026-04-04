#include <gtest/gtest.h>
#include "p2p/servers/relay/relay_server.hpp"
#include "p2p/servers/relay/turn_message.hpp"
#include <boost/asio.hpp>
#include <thread>
#include <chrono>

using namespace p2p::relay;
namespace asio = boost::asio;

class RelayForwardingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Configure relay server
        config_.host = "127.0.0.1";
        config_.port = 9001;
        config_.public_ip = "127.0.0.1";
        config_.min_port = 50000;
        config_.max_port = 50010;
        config_.default_lifetime = 600;
        config_.max_allocations = 10;

        // Create and start relay server
        relay_server_ = std::make_unique<RelayServer>(config_);
        server_thread_ = std::thread([this]() {
            relay_server_->Start();
        });

        // Wait for server to start
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    void TearDown() override {
        if (relay_server_) {
            relay_server_->Stop();
        }
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    }

    RelayServerConfig config_;
    std::unique_ptr<RelayServer> relay_server_;
    std::thread server_thread_;
};

TEST_F(RelayForwardingTest, ClientToPeerForwarding) {
    asio::io_context io_context;

    // Create client socket
    asio::ip::udp::socket client_socket(io_context);
    client_socket.open(asio::ip::udp::v4());
    client_socket.bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), 0));

    // Create peer socket
    asio::ip::udp::socket peer_socket(io_context);
    peer_socket.open(asio::ip::udp::v4());
    peer_socket.bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), 0));

    auto peer_endpoint = peer_socket.local_endpoint();

    // Step 1: Client allocates relay
    StunMessage allocate_req;
    allocate_req.message_type = MessageType::ALLOCATE_REQUEST;
    allocate_req.magic_cookie = MAGIC_COOKIE;
    // ... (add required attributes)

    // Step 2: Client creates permission for peer
    // ... (CREATE_PERMISSION_REQUEST)

    // Step 3: Client sends data via SEND indication
    StunMessage send_ind;
    send_ind.message_type = MessageType::SEND_INDICATION;
    send_ind.magic_cookie = MAGIC_COOKIE;

    // Add peer address
    Address peer_addr(peer_endpoint.address().to_string(), peer_endpoint.port());
    // ... (add XOR-PEER-ADDRESS attribute)

    // Add data
    std::vector<uint8_t> test_data = {0x01, 0x02, 0x03, 0x04, 0x05};
    send_ind.AddAttribute(AttributeType::DATA, test_data);

    // Send to relay server
    auto serialized = send_ind.Serialize();
    asio::ip::udp::endpoint relay_endpoint(
        asio::ip::make_address("127.0.0.1"),
        config_.port);

    client_socket.send_to(asio::buffer(serialized), relay_endpoint);

    // Step 4: Peer receives data
    std::vector<uint8_t> recv_buffer(1500);
    asio::ip::udp::endpoint sender_endpoint;

    auto bytes_received = peer_socket.receive_from(
        asio::buffer(recv_buffer),
        sender_endpoint);

    // Verify data
    EXPECT_GT(bytes_received, 0u);
    EXPECT_EQ(std::vector<uint8_t>(recv_buffer.begin(), recv_buffer.begin() + bytes_received),
              test_data);
}

TEST_F(RelayForwardingTest, PeerToClientForwarding) {
    asio::io_context io_context;

    // Create client socket
    asio::ip::udp::socket client_socket(io_context);
    client_socket.open(asio::ip::udp::v4());
    client_socket.bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), 0));

    // Create peer socket
    asio::ip::udp::socket peer_socket(io_context);
    peer_socket.open(asio::ip::udp::v4());
    peer_socket.bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), 0));

    // Step 1: Setup allocation and permission (similar to above)
    // ...

    // Step 2: Peer sends data to relay address
    std::vector<uint8_t> test_data = {0xAA, 0xBB, 0xCC, 0xDD};
    asio::ip::udp::endpoint relay_addr_endpoint(
        asio::ip::make_address("127.0.0.1"),
        50000);  // Allocated relay port

    peer_socket.send_to(asio::buffer(test_data), relay_addr_endpoint);

    // Step 3: Client receives DATA indication
    std::vector<uint8_t> recv_buffer(1500);
    asio::ip::udp::endpoint sender_endpoint;

    auto bytes_received = client_socket.receive_from(
        asio::buffer(recv_buffer),
        sender_endpoint);

    EXPECT_GT(bytes_received, 0u);

    // Parse DATA indication
    auto indication = StunMessage::Parse(recv_buffer.data(), bytes_received);
    ASSERT_NE(indication, nullptr);
    EXPECT_EQ(indication->message_type, MessageType::DATA_INDICATION);

    // Verify DATA attribute contains original data
    auto data_attr = indication->GetAttribute(AttributeType::DATA);
    ASSERT_NE(data_attr, nullptr);
    EXPECT_EQ(data_attr->value, test_data);

    // Verify XOR-PEER-ADDRESS attribute
    auto peer_attr = indication->GetAttribute(AttributeType::XOR_PEER_ADDRESS);
    ASSERT_NE(peer_attr, nullptr);
}

TEST_F(RelayForwardingTest, BidirectionalExchange) {
    asio::io_context io_context;

    // Setup client and peer sockets
    asio::ip::udp::socket client_socket(io_context);
    client_socket.open(asio::ip::udp::v4());
    client_socket.bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), 0));

    asio::ip::udp::socket peer_socket(io_context);
    peer_socket.open(asio::ip::udp::v4());
    peer_socket.bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), 0));

    // Test data
    std::vector<uint8_t> client_to_peer_data = {0x01, 0x02, 0x03};
    std::vector<uint8_t> peer_to_client_data = {0x04, 0x05, 0x06};

    // Send from client to peer
    // ... (SEND indication)

    // Send from peer to client
    // ... (raw UDP to relay address)

    // Verify both directions work
    // ...
}

TEST_F(RelayForwardingTest, BandwidthLimiting) {
    // Send large amount of data
    // Verify bandwidth limiter throttles correctly
    // Check that some packets are dropped when limit exceeded
}

TEST_F(RelayForwardingTest, PermissionEnforcement) {
    // Try to send data to peer without permission
    // Verify data is not forwarded
    // Add permission
    // Verify data is now forwarded
}

TEST_F(RelayForwardingTest, AllocationExpiry) {
    // Create allocation with short lifetime
    // Start relaying data
    // Wait for allocation to expire
    // Verify relay stops and socket is cleaned up
}

TEST_F(RelayForwardingTest, MultipleConcurrentRelays) {
    // Create multiple allocations
    // Send data through all of them concurrently
    // Verify all data is correctly forwarded
    // Verify no cross-talk between allocations
}

TEST_F(RelayForwardingTest, LargePayloadHandling) {
    // Send maximum UDP payload size (1472 bytes)
    // Verify correct forwarding
    // Try to send oversized payload
    // Verify proper handling
}

TEST_F(RelayForwardingTest, MalformedSendIndication) {
    // Send SEND indication without DATA attribute
    // Verify server doesn't crash
    // Send SEND indication without XOR-PEER-ADDRESS
    // Verify server doesn't crash
}

TEST_F(RelayForwardingTest, StatisticsTracking) {
    // Send data through relay
    // Check allocation statistics
    // Verify bytes_sent and bytes_received are correct
    // Check throughput monitor
}
