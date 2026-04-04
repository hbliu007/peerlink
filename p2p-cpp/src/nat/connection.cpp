#include "p2p/nat/connection.hpp"
#include <stdexcept>

namespace p2p {
namespace nat {

// ============================================================================
// UDPConnection Implementation
// ============================================================================

UDPConnection::UDPConnection(net::UDPSocket socket, const net::SocketAddr& remote_addr)
    : socket_(std::move(socket)),
      remote_addr_(remote_addr),
      connected_(socket_.IsValid()) {
}

UDPConnection::~UDPConnection() {
    close();
}

ssize_t UDPConnection::send(const std::vector<uint8_t>& data) {
    if (!connected_.load()) {
        return -1;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    return socket_.send_to(data, remote_addr_);
}

ssize_t UDPConnection::recv(std::vector<uint8_t>& buffer, size_t max_size) {
    if (!connected_.load()) {
        return -1;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    net::SocketAddr from;
    ssize_t n = socket_.recv_from(buffer, from);

    // For UDP, filter packets by source IP to prevent accepting
    // data from unrelated peers. Allow port changes (NAT remapping).
    if (n > 0) {
        if (from.ip != remote_addr_.ip) {
            buffer.clear();
            return -1;
        }

        // Auto-adapt to NAT port remapping
        if (from.port != remote_addr_.port) {
            remote_addr_ = from;
        }

        // Truncate oversized data
        if (max_size > 0 && buffer.size() > max_size) {
            buffer.resize(max_size);
            n = static_cast<ssize_t>(buffer.size());
        }
    }

    return n;
}

void UDPConnection::close() {
    connected_.store(false);
    std::lock_guard<std::mutex> lock(mutex_);
    socket_.close();
}

std::optional<net::SocketAddr> UDPConnection::get_local_addr() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return socket_.get_local_addr();
}

std::optional<net::SocketAddr> UDPConnection::get_remote_addr() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return remote_addr_;
}

void UDPConnection::update_remote_addr(const net::SocketAddr& addr) {
    std::lock_guard<std::mutex> lock(mutex_);
    remote_addr_ = addr;
}

// ============================================================================
// TCPConnection Implementation
// ============================================================================

TCPConnection::TCPConnection(net::TCPSocket socket)
    : socket_(std::move(socket)) {
}

TCPConnection::~TCPConnection() {
    close();
}

bool TCPConnection::is_connected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return socket_.is_connected();
}

ssize_t TCPConnection::send(const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!socket_.is_connected()) {
        return -1;
    }
    return socket_.send(data);
}

ssize_t TCPConnection::recv(std::vector<uint8_t>& buffer, size_t max_size) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!socket_.is_connected()) {
        return -1;
    }
    return socket_.recv(buffer, max_size);
}

void TCPConnection::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    socket_.close();
}

std::optional<net::SocketAddr> TCPConnection::get_local_addr() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return socket_.get_local_addr();
}

std::optional<net::SocketAddr> TCPConnection::get_remote_addr() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return socket_.get_peer_addr();
}

} // namespace nat
} // namespace p2p
