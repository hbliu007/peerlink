#pragma once

#include "p2p/protocol/stun.hpp"
#include <boost/asio.hpp>
#include <memory>
#include <string>

namespace p2p::server {

using boost::asio::ip::udp;
using boost::asio::ip::tcp;

class StunServer : public std::enable_shared_from_this<StunServer> {
public:
    StunServer(boost::asio::io_context& io_context,
               const std::string& host = "0.0.0.0",
               uint16_t udp_port = 3478,
               uint16_t tcp_port = 3479);

    ~StunServer();

    // Start server
    void start();

    // Stop server
    void stop();

    // Check if running
    bool is_running() const { return running_; }

private:
    // UDP handling
    void start_udp_receive();
    void handle_udp_receive(const boost::system::error_code& error, size_t bytes_transferred);
    void handle_udp_packet(const uint8_t* data, size_t length, const udp::endpoint& remote_endpoint);

    // TCP handling
    void start_tcp_accept();
    void handle_tcp_accept(std::shared_ptr<tcp::socket> socket, const boost::system::error_code& error);
    void handle_tcp_client(std::shared_ptr<tcp::socket> socket);

    // STUN message processing
    std::vector<uint8_t> process_stun_message(
        const uint8_t* data,
        size_t length,
        const std::string& client_ip,
        uint16_t client_port
    );

    std::vector<uint8_t> create_binding_response(
        const protocol::StunMessage& request,
        const std::string& client_ip,
        uint16_t client_port
    );

private:
    boost::asio::io_context& io_context_;
    std::string host_;
    uint16_t udp_port_;
    uint16_t tcp_port_;
    bool running_;

    // UDP
    std::unique_ptr<udp::socket> udp_socket_;
    udp::endpoint udp_remote_endpoint_;
    std::array<uint8_t, 512> udp_recv_buffer_;

    // TCP
    std::unique_ptr<tcp::acceptor> tcp_acceptor_;
};

}  // namespace p2p::server
