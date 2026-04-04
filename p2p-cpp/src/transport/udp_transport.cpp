#include "p2p/transport/udp_transport.hpp"
#include <iostream>

namespace asio = boost::asio;

namespace p2p {
namespace transport {

UDPTransport::UDPTransport(boost::asio::io_context& io_context, uint16_t local_port)
    : io_context_(io_context)
    , socket_(io_context)
    , recv_buffer_(65536)  // Max UDP packet size
    , running_(false)
{
    boost::system::error_code ec;
    socket_.open(udp::v4(), ec);
    if (!ec) {
        socket_.set_option(udp::socket::reuse_address(true), ec);
        socket_.bind(udp::endpoint(udp::v4(), local_port), ec);
    }
}

UDPTransport::~UDPTransport() {
    stop();
}

void UDPTransport::start(std::error_code& ec) {
    if (running_) {
        ec = std::make_error_code(std::errc::already_connected);
        return;
    }

    if (!socket_.is_open()) {
        ec = std::make_error_code(std::errc::not_connected);
        return;
    }

    running_ = true;
    start_receive();
    ec.clear();
}

void UDPTransport::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    boost::system::error_code ec;
    socket_.close(ec);
}

void UDPTransport::send_to(const std::vector<uint8_t>& data,
                           const udp::endpoint& endpoint,
                           SendCallback callback) {
    if (!running_) {
        if (callback) {
            callback(std::make_error_code(std::errc::not_connected));
        }
        return;
    }

    // Keep data alive until async operation completes
    auto data_copy = std::make_shared<std::vector<uint8_t>>(data);
    auto self = shared_from_this();
    socket_.async_send_to(
        boost::asio::buffer(*data_copy),
        endpoint,
        [self, callback, data_copy](const boost::system::error_code& ec, std::size_t /*bytes_sent*/) {
            if (callback) {
                std::error_code std_ec;
                if (ec) {
                    // Map boost error to std error
                    std_ec = std::error_code(ec.value(), std::system_category());
                }
                callback(std_ec);
            }
        }
    );
}

void UDPTransport::set_peer(const udp::endpoint& endpoint) {
    peer_endpoint_ = endpoint;
}

void UDPTransport::send(const std::vector<uint8_t>& data, SendCallback callback) {
    if (peer_endpoint_.port() == 0) {
        if (callback) {
            callback(std::make_error_code(std::errc::destination_address_required));
        }
        return;
    }

    send_to(data, peer_endpoint_, callback);
}

void UDPTransport::set_receive_callback(ReceiveCallback callback) {
    receive_callback_ = callback;
}

udp::endpoint UDPTransport::local_endpoint() const {
    boost::system::error_code ec;
    return socket_.local_endpoint(ec);
}

void UDPTransport::start_receive() {
    if (!running_) {
        return;
    }

    auto self = shared_from_this();
    socket_.async_receive_from(
        boost::asio::buffer(recv_buffer_),
        recv_endpoint_,
        [self](const boost::system::error_code& ec, std::size_t bytes_transferred) {
            self->handle_receive(ec, bytes_transferred);
        }
    );
}

void UDPTransport::handle_receive(const boost::system::error_code& ec,
                                  std::size_t bytes_transferred) {
    if (!running_) {
        return;
    }

    if (!ec && bytes_transferred > 0) {
        if (receive_callback_) {
            std::vector<uint8_t> data(recv_buffer_.begin(),
                                     recv_buffer_.begin() + bytes_transferred);
            receive_callback_(data, recv_endpoint_);
        }
    }

    // Continue receiving
    start_receive();
}

} // namespace transport
} // namespace p2p
