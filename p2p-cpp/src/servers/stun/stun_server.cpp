#include "stun_server.hpp"
#include "p2p/utils/logger.hpp"

namespace asio = boost::asio;

namespace p2p::server {

StunServer::StunServer(boost::asio::io_context& io_context,
                       const std::string& host,
                       uint16_t udp_port,
                       uint16_t tcp_port)
    : io_context_(io_context),
      host_(host),
      udp_port_(udp_port),
      tcp_port_(tcp_port),
      running_(false) {}

StunServer::~StunServer() {
    stop();
}

void StunServer::start() {
    if (running_) {
        return;
    }

    running_ = true;

    // Start UDP server
    try {
        udp_socket_ = std::make_unique<udp::socket>(
            io_context_,
            udp::endpoint(boost::asio::ip::make_address(host_), udp_port_)
        );
        LOG_INFO("UDP STUN server listening on {}:{}", host_, udp_port_);
        start_udp_receive();
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to start UDP server: {}", e.what());
        throw;
    }

    // Start TCP server
    try {
        tcp_acceptor_ = std::make_unique<tcp::acceptor>(
            io_context_,
            tcp::endpoint(boost::asio::ip::make_address(host_), tcp_port_)
        );
        LOG_INFO("TCP STUN server listening on {}:{}", host_, tcp_port_);
        start_tcp_accept();
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to start TCP server: {}", e.what());
        throw;
    }

    LOG_INFO("STUN server started");
}

void StunServer::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    if (udp_socket_) {
        udp_socket_->close();
    }

    if (tcp_acceptor_) {
        tcp_acceptor_->close();
    }

    LOG_INFO("STUN server stopped");
}

// UDP handling
void StunServer::start_udp_receive() {
    udp_socket_->async_receive_from(
        boost::asio::buffer(udp_recv_buffer_),
        udp_remote_endpoint_,
        [this](const boost::system::error_code& error, size_t bytes_transferred) {
            handle_udp_receive(error, bytes_transferred);
        }
    );
}

void StunServer::handle_udp_receive(const boost::system::error_code& error, size_t bytes_transferred) {
    if (!error && running_) {
        handle_udp_packet(udp_recv_buffer_.data(), bytes_transferred, udp_remote_endpoint_);
        start_udp_receive();  // Continue receiving
    }
}

void StunServer::handle_udp_packet(const uint8_t* data, size_t length, const udp::endpoint& remote_endpoint) {
    std::string client_ip = remote_endpoint.address().to_string();
    uint16_t client_port = remote_endpoint.port();

    auto response = process_stun_message(data, length, client_ip, client_port);

    if (!response.empty()) {
        udp_socket_->async_send_to(
            boost::asio::buffer(response),
            remote_endpoint,
            [](const boost::system::error_code&, size_t) {}
        );
    }
}

// TCP handling
void StunServer::start_tcp_accept() {
    auto socket = std::make_shared<tcp::socket>(io_context_);

    tcp_acceptor_->async_accept(
        *socket,
        [this, socket](const boost::system::error_code& error) {
            handle_tcp_accept(socket, error);
        }
    );
}

void StunServer::handle_tcp_accept(std::shared_ptr<tcp::socket> socket, const boost::system::error_code& error) {
    if (!error && running_) {
        // Handle client in separate task
        std::thread([this, socket]() {
            handle_tcp_client(socket);
        }).detach();

        // Continue accepting
        start_tcp_accept();
    }
}

void StunServer::handle_tcp_client(std::shared_ptr<tcp::socket> socket) {
    try {
        // Read STUN message header (20 bytes)
        std::array<uint8_t, 20> header;
        boost::asio::read(*socket, boost::asio::buffer(header));

        // Extract message length
        uint16_t message_length = ntohs(*reinterpret_cast<const uint16_t*>(&header[2]));

        // Read rest of message
        std::vector<uint8_t> data(20 + message_length);
        std::memcpy(data.data(), header.data(), 20);
        boost::asio::read(*socket, boost::asio::buffer(data.data() + 20, message_length));

        // Get client address
        auto remote_endpoint = socket->remote_endpoint();
        std::string client_ip = remote_endpoint.address().to_string();
        uint16_t client_port = remote_endpoint.port();

        // Process message
        auto response = process_stun_message(data.data(), data.size(), client_ip, client_port);

        if (!response.empty()) {
            // Send response with TCP framing (2-byte length prefix)
            uint16_t response_length = htons(static_cast<uint16_t>(response.size()));
            std::vector<uint8_t> framed_response;
            framed_response.insert(framed_response.end(),
                                  reinterpret_cast<const uint8_t*>(&response_length),
                                  reinterpret_cast<const uint8_t*>(&response_length) + 2);
            framed_response.insert(framed_response.end(), response.begin(), response.end());

            boost::asio::write(*socket, boost::asio::buffer(framed_response));
        }
    } catch (const std::exception& e) {
        LOG_ERROR("TCP client error: {}", e.what());
    }
}

// STUN message processing
std::vector<uint8_t> StunServer::process_stun_message(
    const uint8_t* data,
    size_t length,
    const std::string& client_ip,
    uint16_t client_port
) {
    // Parse STUN message
    auto message_opt = protocol::StunMessage::parse(data, length);

    if (!message_opt) {
        LOG_WARN("Invalid STUN message from {}:{}", client_ip, client_port);
        return {};
    }

    auto& message = *message_opt;

    // Handle binding request
    if (message.message_type() == protocol::StunMessageType::BindingRequest) {
        return create_binding_response(message, client_ip, client_port);
    } else {
        // Unknown message type - return error
        auto error_msg = protocol::create_error_response(
            message.transaction_id(),
            protocol::StunErrorCode::BadRequest,
            "Unknown message type"
        );
        return error_msg.serialize();
    }
}

std::vector<uint8_t> StunServer::create_binding_response(
    const protocol::StunMessage& request,
    const std::string& client_ip,
    uint16_t client_port
) {
    // Create response message
    protocol::StunMessage response(
        protocol::StunMessageType::BindingResponse,
        request.transaction_id()
    );

    // Create XOR-MAPPED-ADDRESS attribute
    auto xor_mapped_addr = protocol::create_xor_mapped_address(
        client_ip,
        client_port,
        request.transaction_id()
    );

    response.add_attribute(protocol::StunAttribute(
        protocol::StunAttributeType::XorMappedAddress,
        std::move(xor_mapped_addr)
    ));

    return response.serialize();
}

}  // namespace p2p::server
