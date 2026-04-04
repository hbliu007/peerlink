#pragma once

#include <memory>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include "p2p/net/socket.hpp"

namespace p2p {
namespace nat {

/**
 * Base connection class for NAT traversal results
 */
class Connection {
public:
    virtual ~Connection() = default;

    /**
     * Get connection type
     */
    virtual std::string get_type() const = 0;

    /**
     * Check if connection is active
     */
    virtual bool is_connected() const = 0;

    /**
     * Send data through the connection
     */
    virtual ssize_t send(const std::vector<uint8_t>& data) = 0;

    /**
     * Receive data from the connection
     */
    virtual ssize_t recv(std::vector<uint8_t>& buffer, size_t max_size = 65536) = 0;

    /**
     * Close the connection
     */
    virtual void close() = 0;

    /**
     * Get local address
     */
    virtual std::optional<net::SocketAddr> get_local_addr() const = 0;

    /**
     * Get remote address
     */
    virtual std::optional<net::SocketAddr> get_remote_addr() const = 0;
};

/**
 * UDP Connection wrapper
 * Wraps UDPSocket with remote address for point-to-point communication
 */
class UDPConnection : public Connection {
public:
    UDPConnection(net::UDPSocket socket, const net::SocketAddr& remote_addr);
    ~UDPConnection() override;

    std::string get_type() const override { return "udp"; }
    bool is_connected() const override { return connected_.load(); }
    ssize_t send(const std::vector<uint8_t>& data) override;
    ssize_t recv(std::vector<uint8_t>& buffer, size_t max_size = 65536) override;
    void close() override;
    std::optional<net::SocketAddr> get_local_addr() const override;
    std::optional<net::SocketAddr> get_remote_addr() const override;

    /**
     * Get underlying socket
     */
    net::UDPSocket& get_socket() { return socket_; }

    /**
     * Update remote address (for NAT traversal after punch)
     */
    void update_remote_addr(const net::SocketAddr& addr);

private:
    net::UDPSocket socket_;
    net::SocketAddr remote_addr_;
    std::atomic<bool> connected_;
    mutable std::mutex mutex_;
};

/**
 * TCP Connection wrapper
 * Wraps connected TCPSocket for stream communication
 */
class TCPConnection : public Connection {
public:
    explicit TCPConnection(net::TCPSocket socket);
    ~TCPConnection() override;

    std::string get_type() const override { return "tcp"; }
    bool is_connected() const override;
    ssize_t send(const std::vector<uint8_t>& data) override;
    ssize_t recv(std::vector<uint8_t>& buffer, size_t max_size = 65536) override;
    void close() override;
    std::optional<net::SocketAddr> get_local_addr() const override;
    std::optional<net::SocketAddr> get_remote_addr() const override;

    /**
     * Get underlying socket
     */
    net::TCPSocket& get_socket() { return socket_; }

private:
    net::TCPSocket socket_;
    mutable std::mutex mutex_;
};

} // namespace nat
} // namespace p2p
