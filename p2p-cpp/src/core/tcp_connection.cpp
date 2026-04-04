#include "p2p/core/tcp_connection.hpp"
#include <sstream>

namespace p2p {

std::atomic<ConnectionId> TcpConnection::next_id_{1};

std::shared_ptr<TcpConnection> TcpConnection::Create(
    boost::asio::io_context& io_context,
    const PeerId& peer_id) {
    return std::shared_ptr<TcpConnection>(new TcpConnection(io_context, peer_id));
}

std::shared_ptr<TcpConnection> TcpConnection::Create(
    boost::asio::ip::tcp::socket socket,
    const PeerId& peer_id) {
    return std::shared_ptr<TcpConnection>(new TcpConnection(std::move(socket), peer_id));
}

TcpConnection::TcpConnection(boost::asio::io_context& io_context, const PeerId& peer_id)
    : id_(next_id_++),
      peer_id_(peer_id),
      state_(TransportState::DISCONNECTED),
      socket_(io_context),
      recv_buffer_(8192) {
}

TcpConnection::TcpConnection(boost::asio::ip::tcp::socket socket, const PeerId& peer_id)
    : id_(next_id_++),
      peer_id_(peer_id),
      state_(TransportState::CONNECTED),
      socket_(std::move(socket)),
      recv_buffer_(8192) {
    StartReceive();
}

TcpConnection::~TcpConnection() {
    Close();
}

ConnectionId TcpConnection::GetId() const {
    return id_;
}

const PeerId& TcpConnection::GetPeerId() const {
    return peer_id_;
}

TransportState TcpConnection::GetState() const {
    return state_.load();
}

Status TcpConnection::Send(std::span<const uint8_t> data) {
    if (state_.load() != TransportState::CONNECTED) {
        return Status::Error(StatusCode::ERROR_CONNECTION_FAILED,
                           "Connection not established");
    }

    try {
        std::lock_guard<std::mutex> lock(mutex_);
        boost::system::error_code ec;
        boost::asio::write(socket_, boost::asio::buffer(data.data(), data.size()), ec);

        if (ec) {
            SetState(TransportState::ERROR);
            return Status::Error(StatusCode::ERROR_INTERNAL,
                               "Send failed: " + ec.message());
        }

        return Status::OK();
    } catch (const std::exception& e) {
        SetState(TransportState::ERROR);
        return Status::Error(StatusCode::ERROR_INTERNAL,
                           std::string("Send exception: ") + e.what());
    }
}

void TcpConnection::ReceiveAsync(ReceiveCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    receive_callback_ = std::move(callback);

    if (state_.load() == TransportState::CONNECTED) {
        StartReceive();
    }
}

Status TcpConnection::Close() {
    auto expected_state = state_.load();
    if (expected_state == TransportState::DISCONNECTED) {
        return Status::OK();
    }

    SetState(TransportState::DISCONNECTING);

    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (socket_.is_open()) {
            boost::system::error_code ec;
            socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            socket_.close(ec);
        }
        SetState(TransportState::DISCONNECTED);
        return Status::OK();
    } catch (const std::exception& e) {
        SetState(TransportState::ERROR);
        return Status::Error(StatusCode::ERROR_INTERNAL,
                           std::string("Close exception: ") + e.what());
    }
}

Multiaddr TcpConnection::GetLocalAddr() const {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!socket_.is_open()) {
            return Multiaddr("");
        }

        auto endpoint = socket_.local_endpoint();
        std::ostringstream oss;

        if (endpoint.address().is_v4()) {
            oss << "/ip4/" << endpoint.address().to_string()
                << "/tcp/" << endpoint.port();
        } else {
            oss << "/ip6/" << endpoint.address().to_string()
                << "/tcp/" << endpoint.port();
        }

        return Multiaddr(oss.str());
    } catch (...) {
        return Multiaddr("");
    }
}

Multiaddr TcpConnection::GetRemoteAddr() const {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!socket_.is_open()) {
            return Multiaddr("");
        }

        auto endpoint = socket_.remote_endpoint();
        std::ostringstream oss;

        if (endpoint.address().is_v4()) {
            oss << "/ip4/" << endpoint.address().to_string()
                << "/tcp/" << endpoint.port();
        } else {
            oss << "/ip6/" << endpoint.address().to_string()
                << "/tcp/" << endpoint.port();
        }

        return Multiaddr(oss.str());
    } catch (...) {
        return Multiaddr("");
    }
}

void TcpConnection::ConnectAsync(
    const boost::asio::ip::tcp::endpoint& endpoint,
    std::function<void(Status)> callback) {

    if (state_.load() != TransportState::DISCONNECTED) {
        callback(Status::Error(StatusCode::ERROR_ALREADY_EXISTS,
                             "Connection already established or in progress"));
        return;
    }

    SetState(TransportState::CONNECTING);

    auto self = shared_from_this();
    socket_.async_connect(endpoint,
        [this, self, callback](const boost::system::error_code& ec) {
            if (ec) {
                SetState(TransportState::ERROR);
                callback(Status::Error(StatusCode::ERROR_CONNECTION_FAILED,
                                     "Connect failed: " + ec.message()));
                return;
            }

            SetState(TransportState::CONNECTED);
            StartReceive();
            callback(Status::OK());
        });
}

void TcpConnection::StartReceive() {
    auto self = shared_from_this();
    socket_.async_read_some(
        boost::asio::buffer(recv_buffer_),
        [this, self](const boost::system::error_code& ec, std::size_t bytes_transferred) {
            HandleReceive(ec, bytes_transferred);
        });
}

void TcpConnection::HandleReceive(const boost::system::error_code& ec, std::size_t bytes_transferred) {
    if (ec) {
        if (ec == boost::asio::error::eof || ec == boost::asio::error::connection_reset) {
            SetState(TransportState::DISCONNECTED);
        } else {
            SetState(TransportState::ERROR);
        }

        if (receive_callback_) {
            receive_callback_(
                Status::Error(StatusCode::ERROR_CONNECTION_FAILED, ec.message()),
                std::span<const uint8_t>());
        }
        return;
    }

    if (receive_callback_) {
        receive_callback_(
            Status::OK(),
            std::span<const uint8_t>(recv_buffer_.data(), bytes_transferred));
    }

    // Continue receiving
    if (state_.load() == TransportState::CONNECTED) {
        StartReceive();
    }
}

void TcpConnection::SetState(TransportState new_state) {
    state_.store(new_state);
}

}  // namespace p2p
