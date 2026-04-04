#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <optional>
#include <unordered_map>
#include <sys/socket.h>
#include <netinet/in.h>

// Platform-specific event definitions
#ifdef __linux__
#include <sys/epoll.h>
#elif defined(__APPLE__) || defined(__FreeBSD__)
// Define epoll-like constants for kqueue compatibility
#define EPOLLIN  0x001
#define EPOLLOUT 0x004
#define EPOLLERR 0x008
#define EPOLLHUP 0x010
#define EPOLLET  0x80000000
#endif

namespace p2p {
namespace net {

/**
 * Socket address wrapper
 */
struct SocketAddr {
    std::string ip;
    uint16_t port;

    SocketAddr() : port(0) {}
    SocketAddr(const std::string& ip, uint16_t port) : ip(ip), port(port) {}

    std::string to_string() const;
    sockaddr_in to_sock_addr() const;
    static SocketAddr from_sock_addr(const sockaddr_in& addr);

    // Compatibility wrappers for legacy call sites.
    std::string ToString() const { return to_string(); }
    sockaddr_in ToSockAddr() const { return to_sock_addr(); }
    static SocketAddr FromSockAddr(const sockaddr_in& addr) { return from_sock_addr(addr); }
};

/**
 * UDP Socket for non-blocking datagram communication
 */
class UDPSocket {
public:
    UDPSocket();
    ~UDPSocket();

    // Disable copy
    UDPSocket(const UDPSocket&) = delete;
    UDPSocket& operator=(const UDPSocket&) = delete;

    // Enable move
    UDPSocket(UDPSocket&& other) noexcept;
    UDPSocket& operator=(UDPSocket&& other) noexcept;

    /**
     * Bind to local address
     * @param addr Local address to bind
     * @return true if successful
     */
    bool bind(const SocketAddr& addr);

    /**
     * Send datagram to remote address
     * @param data Data to send
     * @param addr Destination address
     * @return Number of bytes sent, or -1 on error
     */
    ssize_t send_to(const std::vector<uint8_t>& data, const SocketAddr& addr);

    /**
     * Receive datagram from remote address
     * @param buffer Buffer to receive data
     * @param from Source address (output)
     * @return Number of bytes received, or -1 on error
     */
    ssize_t recv_from(std::vector<uint8_t>& buffer, SocketAddr& from);

    /**
     * Close the socket
     */
    void close();

    /**
     * Get local bound address
     */
    std::optional<SocketAddr> get_local_addr() const;

    /**
     * Get socket file descriptor
     */
    int get_fd() const { return fd_; }

    /**
     * Check if socket is valid
     */
    bool is_valid() const { return fd_ >= 0; }

    // Compatibility wrappers for legacy call sites.
    bool Bind(const SocketAddr& addr) { return bind(addr); }
    ssize_t SendTo(const std::vector<uint8_t>& data, const SocketAddr& addr) { return send_to(data, addr); }
    ssize_t RecvFrom(std::vector<uint8_t>& buffer, SocketAddr& from) { return recv_from(buffer, from); }
    void Close() { close(); }
    std::optional<SocketAddr> GetLocalAddr() const { return get_local_addr(); }
    int GetFd() const { return get_fd(); }
    bool IsValid() const { return is_valid(); }

private:
    int fd_;
    bool SetNonBlocking();
};

/**
 * TCP Socket for non-blocking stream communication
 */
class TCPSocket {
public:
    TCPSocket();
    explicit TCPSocket(int fd);  // For accepted connections
    ~TCPSocket();

    // Disable copy
    TCPSocket(const TCPSocket&) = delete;
    TCPSocket& operator=(const TCPSocket&) = delete;

    // Enable move
    TCPSocket(TCPSocket&& other) noexcept;
    TCPSocket& operator=(TCPSocket&& other) noexcept;

    /**
     * Bind to local address
     */
    bool bind(const SocketAddr& addr);

    /**
     * Listen for incoming connections
     * @param backlog Maximum pending connections
     */
    bool listen(int backlog = 128);

    /**
     * Accept incoming connection
     * @param peer_addr Peer address (output)
     * @return New socket for the connection, or nullptr on error
     */
    std::unique_ptr<TCPSocket> accept(SocketAddr& peer_addr);

    /**
     * Connect to remote address (non-blocking)
     * @param addr Remote address
     * @return true if connection initiated, false on error
     */
    bool connect(const SocketAddr& addr);

    /**
     * Check if connection is established
     */
    bool is_connected() const;

    /**
     * Send data
     * @param data Data to send
     * @return Number of bytes sent, or -1 on error
     */
    ssize_t send(const std::vector<uint8_t>& data);

    /**
     * Receive data
     * @param buffer Buffer to receive data
     * @param max_size Maximum bytes to receive
     * @return Number of bytes received, or -1 on error
     */
    ssize_t recv(std::vector<uint8_t>& buffer, size_t max_size = 65536);

    /**
     * Close the socket
     */
    void close();

    /**
     * Get local address
     */
    std::optional<SocketAddr> get_local_addr() const;

    /**
     * Get peer address
     */
    std::optional<SocketAddr> get_peer_addr() const;

    /**
     * Get socket file descriptor
     */
    int get_fd() const { return fd_; }

    /**
     * Check if socket is valid
     */
    bool is_valid() const { return fd_ >= 0; }

    // Compatibility wrappers for legacy call sites.
    bool Bind(const SocketAddr& addr) { return bind(addr); }
    bool Listen(int backlog = 128) { return listen(backlog); }
    std::unique_ptr<TCPSocket> Accept(SocketAddr& peer_addr) { return accept(peer_addr); }
    bool Connect(const SocketAddr& addr) { return connect(addr); }
    bool IsConnected() const { return is_connected(); }
    ssize_t Send(const std::vector<uint8_t>& data) { return send(data); }
    ssize_t Recv(std::vector<uint8_t>& buffer, size_t max_size = 65536) { return recv(buffer, max_size); }
    void Close() { close(); }
    std::optional<SocketAddr> GetLocalAddr() const { return get_local_addr(); }
    std::optional<SocketAddr> GetPeerAddr() const { return get_peer_addr(); }
    int GetFd() const { return get_fd(); }
    bool IsValid() const { return is_valid(); }

private:
    int fd_;
    bool connected_;
    bool SetNonBlocking();
    bool SetReuseAddr();
};

/**
 * Socket event types
 */
enum class SocketEvent {
    READABLE,
    WRITABLE,
    ERROR,
    CLOSED
};

/**
 * Socket event callback
 */
using SocketEventCallback = std::function<void(int fd, SocketEvent event)>;

/**
 * Socket Manager for event-driven IO
 */
class SocketManager {
public:
    SocketManager();
    ~SocketManager();

    // Disable copy
    SocketManager(const SocketManager&) = delete;
    SocketManager& operator=(const SocketManager&) = delete;

    /**
     * Register socket for events
     * @param fd Socket file descriptor
     * @param callback Event callback
     * @param events Events to monitor (EPOLLIN, EPOLLOUT, etc.)
     */
    bool register_socket(int fd, SocketEventCallback callback, uint32_t events);

    /**
     * Unregister socket
     */
    bool unregister(int fd);

    /**
     * Modify socket events
     */
    bool modify(int fd, uint32_t events);

    /**
     * Run event loop (blocking)
     * @param timeout_ms Timeout in milliseconds (-1 for infinite)
     * @return Number of events processed
     */
    int poll(int timeout_ms = -1);

    /**
     * Stop event loop
     */
    void stop();

    /**
     * Check if running
     */
    bool is_running() const { return running_; }

    // Compatibility wrappers for legacy call sites.
    bool Register(int fd, SocketEventCallback callback, uint32_t events) {
        return register_socket(fd, std::move(callback), events);
    }
    bool Unregister(int fd) { return unregister(fd); }
    bool Modify(int fd, uint32_t events) { return modify(fd, events); }
    int Poll(int timeout_ms = -1) { return poll(timeout_ms); }
    void Stop() { stop(); }
    bool IsRunning() const { return is_running(); }

private:
    int epoll_fd_;
    bool running_;
    std::unordered_map<int, SocketEventCallback> callbacks_;
};

}  // namespace net
}  // namespace p2p
