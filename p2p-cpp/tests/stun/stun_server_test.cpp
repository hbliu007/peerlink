#include "stun_server.hpp"
#include "p2p/protocol/stun.hpp"
#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <thread>
#include <chrono>

using namespace p2p::server;
using namespace p2p::protocol;
using boost::asio::ip::udp;
using boost::asio::ip::tcp;

class StunServerTest : public ::testing::Test {
protected:
    std::unique_ptr<boost::asio::io_context> io_context_;
    std::unique_ptr<StunServer> server_;
    std::thread server_thread_;

    void SetUp() override {
        io_context_ = std::make_unique<boost::asio::io_context>();
        server_ = std::make_unique<StunServer>(*io_context_, "127.0.0.1", 13478, 13479);

        // Start server in separate thread
        server_->start();
        server_thread_ = std::thread([this]() {
            io_context_->run();
        });

        // Give server time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TearDown() override {
        server_->stop();
        io_context_->stop();
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    }
};

TEST_F(StunServerTest, UDPBindingRequest) {
    boost::asio::io_context client_io;
    udp::socket socket(client_io, udp::endpoint(udp::v4(), 0));

    // Create binding request
    TransactionId tid;
    for (size_t i = 0; i < 12; ++i) {
        tid[i] = static_cast<uint8_t>(i + 1);
    }

    StunMessage request(StunMessageType::BindingRequest, tid);
    auto request_data = request.serialize();

    // Send request
    udp::endpoint server_endpoint(
        boost::asio::ip::make_address("127.0.0.1"),
        13478
    );

    socket.send_to(boost::asio::buffer(request_data), server_endpoint);

    // Receive response
    std::array<uint8_t, 512> recv_buffer;
    udp::endpoint sender_endpoint;

    boost::system::error_code error;
    size_t len = socket.receive_from(
        boost::asio::buffer(recv_buffer),
        sender_endpoint,
        0,
        error
    );

    ASSERT_FALSE(error);
    ASSERT_GT(len, 0);

    // Parse response
    auto response = StunMessage::parse(recv_buffer.data(), len);

    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->message_type(), StunMessageType::BindingResponse);
    EXPECT_EQ(response->transaction_id(), tid);

    // Check XOR-MAPPED-ADDRESS attribute
    auto xor_attr = response->get_attribute(StunAttributeType::XorMappedAddress);
    ASSERT_TRUE(xor_attr.has_value());

    auto addr = parse_xor_mapped_address((*xor_attr)->value, tid);
    ASSERT_TRUE(addr.has_value());

    // Should return our local address
    EXPECT_EQ(addr->first, "127.0.0.1");
}

TEST_F(StunServerTest, InvalidMessage) {
    boost::asio::io_context client_io;
    udp::socket socket(client_io, udp::endpoint(udp::v4(), 0));

    // Send invalid data
    std::vector<uint8_t> invalid_data = {0x00, 0x01, 0x02, 0x03};

    udp::endpoint server_endpoint(
        boost::asio::ip::make_address("127.0.0.1"),
        13478
    );

    socket.send_to(boost::asio::buffer(invalid_data), server_endpoint);

    // Should not receive response for invalid message
    std::array<uint8_t, 512> recv_buffer;
    udp::endpoint sender_endpoint;

    socket.non_blocking(true);
    boost::system::error_code error;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    size_t len = socket.receive_from(
        boost::asio::buffer(recv_buffer),
        sender_endpoint,
        0,
        error
    );

    // Should timeout or get no data
    EXPECT_TRUE(error || len == 0);
}

TEST_F(StunServerTest, TCPBindingRequest) {
    boost::asio::io_context client_io;
    tcp::socket socket(client_io);

    // Connect to server
    tcp::endpoint server_endpoint(
        boost::asio::ip::make_address("127.0.0.1"),
        13479
    );

    boost::system::error_code connect_error;
    socket.connect(server_endpoint, connect_error);
    ASSERT_FALSE(connect_error) << "Failed to connect: " << connect_error.message();

    // Create binding request
    TransactionId tid;
    for (size_t i = 0; i < 12; ++i) {
        tid[i] = static_cast<uint8_t>(i + 10);
    }

    StunMessage request(StunMessageType::BindingRequest, tid);
    auto request_data = request.serialize();

    // Send request (TCP uses framing)
    boost::asio::write(socket, boost::asio::buffer(request_data));

    // Read TCP framing length prefix (2 bytes) with retry for timing issues
    std::array<uint8_t, 2> length_prefix;
    boost::system::error_code read_error;

    // Use read_some with timeout to handle potential server delays
    size_t bytes_read = boost::asio::read(socket, boost::asio::buffer(length_prefix), read_error);
    ASSERT_FALSE(read_error) << "Failed to read length prefix: " << read_error.message();
    ASSERT_EQ(bytes_read, 2) << "Incomplete length prefix read";

    uint16_t framed_length = ntohs(*reinterpret_cast<const uint16_t*>(length_prefix.data()));

    // Read the complete STUN message
    std::vector<uint8_t> response_data(framed_length);
    boost::asio::read(socket, boost::asio::buffer(response_data), read_error);
    ASSERT_FALSE(read_error) << "Failed to read response: " << read_error.message();

    // Parse response
    auto response = StunMessage::parse(response_data.data(), response_data.size());

    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->message_type(), StunMessageType::BindingResponse);
    EXPECT_EQ(response->transaction_id(), tid);

    // Check XOR-MAPPED-ADDRESS attribute
    auto xor_attr = response->get_attribute(StunAttributeType::XorMappedAddress);
    ASSERT_TRUE(xor_attr.has_value());

    auto addr = parse_xor_mapped_address((*xor_attr)->value, tid);
    ASSERT_TRUE(addr.has_value());
    EXPECT_EQ(addr->first, "127.0.0.1");
}

TEST_F(StunServerTest, UnknownMessageType) {
    boost::asio::io_context client_io;
    udp::socket socket(client_io, udp::endpoint(udp::v4(), 0));

    // Create message with unknown type
    TransactionId tid;
    for (size_t i = 0; i < 12; ++i) {
        tid[i] = static_cast<uint8_t>(i + 20);
    }

    // Manually create message with invalid type
    std::vector<uint8_t> request_data;

    // Message type (unknown: 0x0999)
    uint16_t type = htons(0x0999);
    request_data.insert(request_data.end(),
                       reinterpret_cast<const uint8_t*>(&type),
                       reinterpret_cast<const uint8_t*>(&type) + 2);

    // Message length (0)
    uint16_t length = htons(0);
    request_data.insert(request_data.end(),
                       reinterpret_cast<const uint8_t*>(&length),
                       reinterpret_cast<const uint8_t*>(&length) + 2);

    // Magic cookie
    uint32_t cookie = htonl(STUN_MAGIC_COOKIE);
    request_data.insert(request_data.end(),
                       reinterpret_cast<const uint8_t*>(&cookie),
                       reinterpret_cast<const uint8_t*>(&cookie) + 4);

    // Transaction ID
    request_data.insert(request_data.end(), tid.begin(), tid.end());

    // Send request
    udp::endpoint server_endpoint(
        boost::asio::ip::make_address("127.0.0.1"),
        13478
    );

    socket.send_to(boost::asio::buffer(request_data), server_endpoint);

    // Receive error response
    std::array<uint8_t, 512> recv_buffer;
    udp::endpoint sender_endpoint;

    boost::system::error_code error;
    size_t len = socket.receive_from(
        boost::asio::buffer(recv_buffer),
        sender_endpoint,
        0,
        error
    );

    ASSERT_FALSE(error);
    ASSERT_GT(len, 0);

    // Parse response
    auto response = StunMessage::parse(recv_buffer.data(), len);

    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->message_type(), StunMessageType::BindingErrorResponse);
    EXPECT_EQ(response->transaction_id(), tid);

    // Check ERROR-CODE attribute
    auto error_attr = response->get_attribute(StunAttributeType::ErrorCode);
    ASSERT_TRUE(error_attr.has_value());
}

TEST_F(StunServerTest, MultipleRequests) {
    boost::asio::io_context client_io;
    udp::socket socket(client_io, udp::endpoint(udp::v4(), 0));

    udp::endpoint server_endpoint(
        boost::asio::ip::make_address("127.0.0.1"),
        13478
    );

    // Send multiple requests
    const size_t num_requests = 10;
    std::vector<TransactionId> tids;

    for (size_t i = 0; i < num_requests; ++i) {
        TransactionId tid;
        for (size_t j = 0; j < 12; ++j) {
            tid[j] = static_cast<uint8_t>((i * 12 + j) & 0xFF);
        }
        tids.push_back(tid);

        StunMessage request(StunMessageType::BindingRequest, tid);
        auto request_data = request.serialize();

        socket.send_to(boost::asio::buffer(request_data), server_endpoint);
    }

    // Receive all responses
    size_t received = 0;
    for (size_t i = 0; i < num_requests; ++i) {
        std::array<uint8_t, 512> recv_buffer;
        udp::endpoint sender_endpoint;

        boost::system::error_code error;
        size_t len = socket.receive_from(
            boost::asio::buffer(recv_buffer),
            sender_endpoint,
            0,
            error
        );

        if (!error && len > 0) {
            auto response = StunMessage::parse(recv_buffer.data(), len);
            if (response && response->message_type() == StunMessageType::BindingResponse) {
                received++;
            }
        }
    }

    EXPECT_EQ(received, num_requests);
}

TEST_F(StunServerTest, IPv6Support) {
    // Test XOR-MAPPED-ADDRESS with IPv6
    TransactionId tid;
    for (size_t i = 0; i < 12; ++i) {
        tid[i] = static_cast<uint8_t>(i);
    }

    std::string ipv6_addr = "2001:db8::1";
    uint16_t port = 54321;

    // Create XOR-MAPPED-ADDRESS for IPv6
    auto xor_data = create_xor_mapped_address(ipv6_addr, port, tid);

    ASSERT_FALSE(xor_data.empty());
    EXPECT_GT(xor_data.size(), 8);  // IPv6 should be larger

    // Parse back
    auto result = parse_xor_mapped_address(xor_data, tid);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first, ipv6_addr);
    EXPECT_EQ(result->second, port);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
