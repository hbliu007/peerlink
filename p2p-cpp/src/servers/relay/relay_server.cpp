/**
 * @file relay_server.cpp
 * @brief Relay Server Implementation
 */

#include "p2p/servers/relay/relay_server.hpp"
#include "p2p/utils/logger.hpp"
#include <csignal>
#include <random>
#include <boost/system/error_code.hpp>

namespace asio = boost::asio;
using error_code = boost::system::error_code;

namespace p2p {
namespace relay {

RelayServer::RelayServer(const RelayServerConfig& config)
    : config_(config),
      allocation_manager_(std::make_unique<AllocationManager>(
          config.min_port,
          config.max_port,
          config.default_lifetime,
          config.max_allocations)),
      bandwidth_limiter_(std::make_unique<BandwidthLimiter>(config.bandwidth_limit)),
      rate_limiter_(std::make_unique<RateLimiter>(config.rate_limit)),
      throughput_monitor_(std::make_unique<ThroughputMonitor>()),
      io_context_(std::make_unique<asio::io_context>()) {
}

RelayServer::~RelayServer() {
    Stop();
}

void RelayServer::Start() {
    if (running_.exchange(true)) {
        return;  // Already running
    }

    // Start allocation manager
    allocation_manager_->Start();

    // Start control channel
    StartControlChannel();

    // Start IO threads
    size_t num_threads = std::thread::hardware_concurrency();
    for (size_t i = 0; i < num_threads; ++i) {
        io_threads_.emplace_back([this]() {
            io_context_->run();
        });
    }

    LOG_INFO("Relay server started on {}:{}", config_.host, config_.port);
}

void RelayServer::Stop() {
    if (!running_.exchange(false)) {
        return;  // Already stopped
    }

    // Stop allocation manager
    allocation_manager_->Stop();

    // Stop IO context
    io_context_->stop();

    // Join IO threads
    for (auto& thread : io_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    // Close sockets
    if (control_socket_) {
        control_socket_->close();
    }

    {
        std::lock_guard<std::mutex> lock(relay_sockets_mutex_);
        for (auto& [port, socket] : relay_sockets_) {
            socket->close();
        }
        relay_sockets_.clear();
    }

    LOG_INFO("Relay server stopped");
}

void RelayServer::StartControlChannel() {
    try {
        asio::ip::udp::endpoint endpoint(
            asio::ip::make_address(config_.host),
            config_.port);

        control_socket_ = std::make_unique<asio::ip::udp::socket>(
            *io_context_,
            endpoint);

        ControlLoop();

        LOG_INFO("Control channel listening on {}:{}", config_.host, config_.port);
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to start control channel: {}", e.what());
        throw;
    }
}

void RelayServer::ControlLoop() {
    auto buffer = std::make_shared<std::array<uint8_t, 2048>>();
    auto endpoint = std::make_shared<asio::ip::udp::endpoint>();

    control_socket_->async_receive_from(
        asio::buffer(*buffer),
        *endpoint,
        [this, buffer, endpoint](const boost::system::error_code& error, std::size_t bytes_received) {
            if (!error && bytes_received > 0) {
                HandleControlMessage(buffer->data(), bytes_received, *endpoint);
            }

            if (running_) {
                ControlLoop();  // Continue receiving
            }
        });
}

void RelayServer::HandleControlMessage(
    const uint8_t* data,
    size_t len,
    const asio::ip::udp::endpoint& client_endpoint) {

    Address client_addr(
        client_endpoint.address().to_string(),
        client_endpoint.port());

    // Check rate limit
    std::string client_key = client_addr.ToString();
    if (!rate_limiter_->AllowRequest(client_key)) {
        // Rate limited - send error response
        std::vector<uint8_t> error_response = CreateErrorResponse(
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  // Empty transaction ID
            ErrorCode::SERVER_ERROR,
            "Rate limit exceeded")->Serialize();

        control_socket_->async_send_to(
            asio::buffer(error_response),
            client_endpoint,
            [](const boost::system::error_code&, std::size_t) {});
        return;
    }

    // Parse STUN message
    auto message = StunMessage::Parse(data, len);
    if (!message) {
        return;  // Invalid message
    }

    std::vector<uint8_t> response;

    // Route based on message type
    switch (message->message_type) {
        case MessageType::BINDING_REQUEST:
            response = HandleBindingRequest(*message, client_addr);
            break;

        case MessageType::ALLOCATE_REQUEST:
            response = HandleAllocateRequest(*message, client_addr);
            break;

        case MessageType::REFRESH_REQUEST:
            response = HandleRefreshRequest(*message, client_addr);
            break;

        case MessageType::CREATE_PERMISSION_REQUEST:
            response = HandleCreatePermissionRequest(*message, client_addr);
            break;

        case MessageType::SEND_INDICATION:
            HandleSendIndication(*message, client_addr);
            return;  // No response for indications

        default:
            // Unknown message type
            response = CreateErrorResponse(
                message->transaction_id,
                ErrorCode::BAD_REQUEST,
                "Unknown message type")->Serialize();
            break;
    }

    // Send response
    if (!response.empty()) {
        control_socket_->async_send_to(
            asio::buffer(response),
            client_endpoint,
            [](const boost::system::error_code&, std::size_t) {});
    }
}

std::vector<uint8_t> RelayServer::HandleBindingRequest(
    const StunMessage& request,
    const Address& client_addr) {

    // Create binding response with XOR-MAPPED-ADDRESS
    StunMessage response;
    response.message_type = MessageType::BINDING_RESPONSE;
    response.magic_cookie = MAGIC_COOKIE;
    response.transaction_id = request.transaction_id;

    auto xor_mapped = CreateXorAddressAttr(client_addr, request.transaction_id);
    response.AddAttribute(AttributeType::XOR_MAPPED_ADDRESS, std::move(xor_mapped));

    return response.Serialize();
}

std::vector<uint8_t> RelayServer::HandleAllocateRequest(
    const StunMessage& request,
    const Address& client_addr) {

    // Check if client already has allocation
    auto existing = allocation_manager_->GetAllocationByClient(client_addr);
    if (existing) {
        return CreateErrorResponse(
            request.transaction_id,
            ErrorCode::ALLOCATION_MISMATCH,
            "Client already has allocation")->Serialize();
    }

    // Get requested lifetime
    uint32_t lifetime = 0;
    auto lifetime_attr = request.GetAttribute(AttributeType::LIFETIME);
    if (lifetime_attr) {
        lifetime = ParseLifetimeAttr(lifetime_attr->value);
    }

    // Create allocation
    auto allocation = allocation_manager_->CreateAllocation(
        client_addr,
        config_.public_ip,
        TransportProtocol::UDP,
        lifetime);

    if (!allocation) {
        return CreateErrorResponse(
            request.transaction_id,
            ErrorCode::INSUFFICIENT_CAPACITY,
            "No resources available")->Serialize();
    }

    // Create relay socket for this allocation
    CreateRelaySocket(allocation);

    // Build success response
    StunMessage response;
    response.message_type = MessageType::ALLOCATE_RESPONSE;
    response.magic_cookie = MAGIC_COOKIE;
    response.transaction_id = request.transaction_id;

    auto xor_relayed = CreateXorAddressAttr(
        allocation->GetRelayAddr(),
        request.transaction_id);
    response.AddAttribute(AttributeType::XOR_RELAYED_ADDRESS, std::move(xor_relayed));

    auto lifetime_value = CreateLifetimeAttr(allocation->GetRemainingTime());
    response.AddAttribute(AttributeType::LIFETIME, std::move(lifetime_value));

    auto xor_mapped = CreateXorAddressAttr(client_addr, request.transaction_id);
    response.AddAttribute(AttributeType::XOR_MAPPED_ADDRESS, std::move(xor_mapped));

    LOG_INFO("Created allocation {} for {}", allocation->GetAllocationId(), client_addr.ToString());

    return response.Serialize();
}

std::vector<uint8_t> RelayServer::HandleRefreshRequest(
    const StunMessage& request,
    const Address& client_addr) {

    auto allocation = allocation_manager_->GetAllocationByClient(client_addr);
    if (!allocation) {
        return CreateErrorResponse(
            request.transaction_id,
            ErrorCode::ALLOCATION_MISMATCH,
            "No allocation found")->Serialize();
    }

    // Get requested lifetime
    uint32_t lifetime = 0;
    auto lifetime_attr = request.GetAttribute(AttributeType::LIFETIME);
    if (lifetime_attr) {
        lifetime = ParseLifetimeAttr(lifetime_attr->value);
    }

    // Refresh allocation
    allocation_manager_->RefreshAllocation(allocation->GetAllocationId(), lifetime);

    // Build response
    StunMessage response;
    response.message_type = MessageType::REFRESH_RESPONSE;
    response.magic_cookie = MAGIC_COOKIE;
    response.transaction_id = request.transaction_id;

    auto lifetime_value = CreateLifetimeAttr(allocation->GetRemainingTime());
    response.AddAttribute(AttributeType::LIFETIME, std::move(lifetime_value));

    return response.Serialize();
}

std::vector<uint8_t> RelayServer::HandleCreatePermissionRequest(
    const StunMessage& request,
    const Address& client_addr) {

    auto allocation = allocation_manager_->GetAllocationByClient(client_addr);
    if (!allocation) {
        return CreateErrorResponse(
            request.transaction_id,
            ErrorCode::ALLOCATION_MISMATCH,
            "No allocation found")->Serialize();
    }

    // Process all XOR-PEER-ADDRESS attributes
    for (const auto& attr : request.attributes) {
        if (attr.type == AttributeType::XOR_PEER_ADDRESS) {
            auto peer_addr = ParseXorAddressAttr(attr.value, request.transaction_id);
            allocation->AddPermission(peer_addr);
        }
    }

    // Success response
    StunMessage response;
    response.message_type = MessageType::CREATE_PERMISSION_RESPONSE;
    response.magic_cookie = MAGIC_COOKIE;
    response.transaction_id = request.transaction_id;

    return response.Serialize();
}

void RelayServer::HandleSendIndication(
    const StunMessage& request,
    const Address& client_addr) {

    auto allocation = allocation_manager_->GetAllocationByClient(client_addr);
    if (!allocation) {
        return;  // Indications don't get error responses
    }

    // Get DATA attribute
    auto data_attr = request.GetAttribute(AttributeType::DATA);
    if (!data_attr) {
        return;
    }

    // Get peer address
    auto peer_attr = request.GetAttribute(AttributeType::XOR_PEER_ADDRESS);
    if (!peer_attr) {
        return;
    }

    auto peer_addr = ParseXorAddressAttr(peer_attr->value, request.transaction_id);

    // Check permission
    if (!allocation->HasPermission(peer_addr)) {
        return;
    }

    // Check bandwidth
    if (!bandwidth_limiter_->ThrottleWrite(
            allocation->GetAllocationId(),
            data_attr->value.size())) {
        return;
    }

    // Record statistics
    allocation->RecordSent(data_attr->value.size());
    throughput_monitor_->RecordWrite(data_attr->value.size());

    // Relay data to peer via relay socket
    std::shared_ptr<asio::ip::udp::socket> relay_socket;
    {
        std::lock_guard<std::mutex> lock(relay_sockets_mutex_);
        auto it = relay_sockets_.find(allocation->GetRelayAddr().port);
        if (it == relay_sockets_.end()) {
            return;  // Relay socket not found
        }
        relay_socket = it->second;
    }

    // Convert peer address to endpoint
    asio::ip::udp::endpoint peer_endpoint(
        asio::ip::make_address(peer_addr.ip),
        peer_addr.port);

    // Send data to peer (unwrapped, raw data)
    auto data_copy = std::make_shared<std::vector<uint8_t>>(data_attr->value);
    relay_socket->async_send_to(
        asio::buffer(*data_copy),
        peer_endpoint,
        [data_copy](const boost::system::error_code& ec, std::size_t /*bytes_sent*/) {
            if (ec) {
                LOG_ERROR("Failed to relay data to peer: {}", ec.message());
            }
        });
}

void RelayServer::CreateRelaySocket(std::shared_ptr<TurnAllocation> allocation) {
    try {
        auto socket = std::make_shared<asio::ip::udp::socket>(
            *io_context_,
            asio::ip::udp::endpoint(
                asio::ip::udp::v4(),
                allocation->GetRelayAddr().port));

        {
            std::lock_guard<std::mutex> lock(relay_sockets_mutex_);
            relay_sockets_[allocation->GetRelayAddr().port] = socket;
        }

        // Start relay loop for this socket
        RelayLoop(allocation, socket);

        LOG_INFO("Created relay socket on port {}", allocation->GetRelayAddr().port);
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to create relay socket: {}", e.what());
    }
}

void RelayServer::RelayLoop(
    std::shared_ptr<TurnAllocation> allocation,
    std::shared_ptr<asio::ip::udp::socket> socket) {

    auto buffer = std::make_shared<std::array<uint8_t, 1500>>();
    auto endpoint = std::make_shared<asio::ip::udp::endpoint>();

    socket->async_receive_from(
        asio::buffer(*buffer),
        *endpoint,
        [this, allocation, socket, buffer, endpoint](
            const boost::system::error_code& error,
            std::size_t bytes_received) {

            if (!error && bytes_received > 0) {
                // Check bandwidth
                if (bandwidth_limiter_->ThrottleRead(
                        allocation->GetAllocationId(),
                        bytes_received)) {

                    // Update statistics
                    allocation->RecordReceived(bytes_received);
                    throughput_monitor_->RecordRead(bytes_received);

                    // Send DATA indication back to client
                    // Wrap the data in a STUN DATA indication message
                    SendDataIndication(
                        allocation,
                        *endpoint,
                        std::vector<uint8_t>(buffer->begin(), buffer->begin() + bytes_received));
                }
            }

            // Continue receiving if allocation is still valid
            if (!allocation->IsExpired() && running_) {
                RelayLoop(allocation, socket);
            } else {
                // Cleanup
                std::lock_guard<std::mutex> lock(relay_sockets_mutex_);
                relay_sockets_.erase(allocation->GetRelayAddr().port);
            }
        });
}


void RelayServer::SendDataIndication(
    std::shared_ptr<TurnAllocation> allocation,
    const asio::ip::udp::endpoint& peer_endpoint,
    const std::vector<uint8_t>& data) {

    // Create DATA indication message
    StunMessage indication;
    indication.message_type = MessageType::DATA_INDICATION;
    indication.magic_cookie = MAGIC_COOKIE;

    // Generate random transaction ID
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint8_t> dis(0, 255);
    for (auto& byte : indication.transaction_id) {
        byte = dis(gen);
    }

    // Add XOR-PEER-ADDRESS attribute (where the data came from)
    Address peer_addr(peer_endpoint.address().to_string(), peer_endpoint.port());
    auto xor_peer = CreateXorAddressAttr(peer_addr, indication.transaction_id);
    indication.AddAttribute(AttributeType::XOR_PEER_ADDRESS, std::move(xor_peer));

    // Add DATA attribute (the actual payload)
    indication.AddAttribute(AttributeType::DATA, data);

    // Serialize the indication
    auto serialized = indication.Serialize();

    // Send to client via control socket
    Address client_addr = allocation->GetClientAddr();
    asio::ip::udp::endpoint client_endpoint(
        asio::ip::make_address(client_addr.ip),
        client_addr.port);

    auto data_copy = std::make_shared<std::vector<uint8_t>>(std::move(serialized));
    control_socket_->async_send_to(
        asio::buffer(*data_copy),
        client_endpoint,
        [data_copy](const boost::system::error_code& ec, std::size_t /*bytes_sent*/) {
            if (ec) {
                LOG_ERROR("Failed to send DATA indication to client: {}", ec.message());
            }
        });
}

RelayServer::Stats RelayServer::GetStats() const {
    Stats stats;
    stats.allocations = allocation_manager_->GetStats();
    stats.bandwidth = bandwidth_limiter_->GetGlobalStats();
    stats.rate_limit = rate_limiter_->GetStats();

    {
        std::lock_guard<std::mutex> lock(relay_sockets_mutex_);
        stats.relay_sockets = relay_sockets_.size();
    }

    return stats;
}

} // namespace relay
} // namespace p2p
