#include "p2p/net/socket.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <cstring>
#include <errno.h>

#ifdef __linux__
#include <sys/epoll.h>
#elif defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/event.h>
#endif

namespace p2p {
namespace net {

// SocketAddr implementation
std::string SocketAddr::to_string() const {
    return ip + ":" + std::to_string(port);
}

sockaddr_in SocketAddr::to_sock_addr() const {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
    return addr;
}

SocketAddr SocketAddr::from_sock_addr(const sockaddr_in& addr) {
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, ip_str, sizeof(ip_str));
    return SocketAddr(ip_str, ntohs(addr.sin_port));
}

// UDPSocket implementation
UDPSocket::UDPSocket() : fd_(-1) {
    fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_ >= 0) {
        SetNonBlocking();
    }
}

UDPSocket::~UDPSocket() {
    close();
}

UDPSocket::UDPSocket(UDPSocket&& other) noexcept : fd_(other.fd_) {
    other.fd_ = -1;
}

UDPSocket& UDPSocket::operator=(UDPSocket&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

bool UDPSocket::bind(const SocketAddr& addr) {
    if (fd_ < 0) return false;

    auto sockaddr = addr.to_sock_addr();
    return (::bind)(fd_, reinterpret_cast<struct sockaddr*>(&sockaddr), sizeof(sockaddr)) == 0;
}

ssize_t UDPSocket::send_to(const std::vector<uint8_t>& data, const SocketAddr& addr) {
    if (fd_ < 0 || data.empty()) return -1;

    auto sockaddr = addr.to_sock_addr();
    return sendto(fd_, data.data(), data.size(), 0,
                  reinterpret_cast<struct sockaddr*>(&sockaddr), sizeof(sockaddr));
}

ssize_t UDPSocket::recv_from(std::vector<uint8_t>& buffer, SocketAddr& from) {
    if (fd_ < 0) return -1;

    buffer.resize(65536);  // Max UDP packet size
    sockaddr_in from_addr{};
    socklen_t addr_len = sizeof(from_addr);

    ssize_t n = recvfrom(fd_, buffer.data(), buffer.size(), 0,
                         reinterpret_cast<struct sockaddr*>(&from_addr), &addr_len);

    if (n > 0) {
        buffer.resize(n);
        from = SocketAddr::from_sock_addr(from_addr);
    }

    return n;
}

void UDPSocket::close() {
    if (fd_ >= 0) {
        (::close)(fd_);
        fd_ = -1;
    }
}

std::optional<SocketAddr> UDPSocket::get_local_addr() const {
    if (fd_ < 0) return std::nullopt;

    sockaddr_in addr{};
    socklen_t addr_len = sizeof(addr);
    if (getsockname(fd_, reinterpret_cast<struct sockaddr*>(&addr), &addr_len) == 0) {
        return SocketAddr::from_sock_addr(addr);
    }
    return std::nullopt;
}

bool UDPSocket::SetNonBlocking() {
    if (fd_ < 0) return false;
    int flags = fcntl(fd_, F_GETFL, 0);
    return fcntl(fd_, F_SETFL, flags | O_NONBLOCK) == 0;
}

// TCPSocket implementation
TCPSocket::TCPSocket() : fd_(-1), connected_(false) {
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ >= 0) {
        SetNonBlocking();
        SetReuseAddr();
    }
}

TCPSocket::TCPSocket(int fd) : fd_(fd), connected_(true) {
    if (fd_ >= 0) {
        SetNonBlocking();
    }
}

TCPSocket::~TCPSocket() {
    close();
}

TCPSocket::TCPSocket(TCPSocket&& other) noexcept
    : fd_(other.fd_), connected_(other.connected_) {
    other.fd_ = -1;
    other.connected_ = false;
}

TCPSocket& TCPSocket::operator=(TCPSocket&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = other.fd_;
        connected_ = other.connected_;
        other.fd_ = -1;
        other.connected_ = false;
    }
    return *this;
}

bool TCPSocket::bind(const SocketAddr& addr) {
    if (fd_ < 0) return false;

    auto sockaddr = addr.to_sock_addr();
    return (::bind)(fd_, reinterpret_cast<struct sockaddr*>(&sockaddr), sizeof(sockaddr)) == 0;
}

bool TCPSocket::listen(int backlog) {
    if (fd_ < 0) return false;
    return (::listen)(fd_, backlog) == 0;
}

std::unique_ptr<TCPSocket> TCPSocket::accept(SocketAddr& peer_addr) {
    if (fd_ < 0) return nullptr;

    sockaddr_in addr{};
    socklen_t addr_len = sizeof(addr);
    int client_fd = (::accept)(fd_, reinterpret_cast<struct sockaddr*>(&addr), &addr_len);

    if (client_fd < 0) return nullptr;

    peer_addr = SocketAddr::from_sock_addr(addr);
    return std::make_unique<TCPSocket>(client_fd);
}

bool TCPSocket::connect(const SocketAddr& addr) {
    if (fd_ < 0) return false;

    auto sockaddr = addr.to_sock_addr();
    int result = (::connect)(fd_, reinterpret_cast<struct sockaddr*>(&sockaddr), sizeof(sockaddr));

    if (result == 0) {
        connected_ = true;
        return true;
    }

    // Non-blocking connect returns EINPROGRESS
    if (errno == EINPROGRESS || errno == EINTR) {
        // Connection in progress, will complete asynchronously
        return true;
    }

    return false;
}

bool TCPSocket::is_connected() const {
    if (fd_ < 0) return false;

    // If already marked as connected, verify it's still valid
    if (connected_) {
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(fd_, SOL_SOCKET, SO_ERROR, &error, &len) == 0) {
            return error == 0;
        }
        return false;
    }

    // For non-blocking connections that returned EINPROGRESS,
    // verify connection completed by checking for a valid peer address.
    // A socket that never called connect() has no peer and should return false.
    sockaddr_in peer{};
    socklen_t peer_len = sizeof(peer);
    if (getpeername(fd_, reinterpret_cast<struct sockaddr*>(&peer), &peer_len) != 0) {
        return false;
    }

    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(fd_, SOL_SOCKET, SO_ERROR, &error, &len) == 0) {
        if (error == 0) {
            // Connection successful, update state
            const_cast<TCPSocket*>(this)->connected_ = true;
            return true;
        }
    }

    return false;
}

ssize_t TCPSocket::send(const std::vector<uint8_t>& data) {
    if (fd_ < 0 || data.empty()) return -1;
    return (::send)(fd_, data.data(), data.size(), 0);
}

ssize_t TCPSocket::recv(std::vector<uint8_t>& buffer, size_t max_size) {
    if (fd_ < 0) return -1;

    buffer.resize(max_size);
    ssize_t n = (::recv)(fd_, buffer.data(), buffer.size(), 0);

    if (n > 0) {
        buffer.resize(n);
    } else if (n == 0) {
        connected_ = false;  // Connection closed
    }

    return n;
}

void TCPSocket::close() {
    if (fd_ >= 0) {
        (::close)(fd_);
        fd_ = -1;
        connected_ = false;
    }
}

std::optional<SocketAddr> TCPSocket::get_local_addr() const {
    if (fd_ < 0) return std::nullopt;

    sockaddr_in addr{};
    socklen_t addr_len = sizeof(addr);
    if (getsockname(fd_, reinterpret_cast<struct sockaddr*>(&addr), &addr_len) == 0) {
        return SocketAddr::from_sock_addr(addr);
    }
    return std::nullopt;
}

std::optional<SocketAddr> TCPSocket::get_peer_addr() const {
    if (fd_ < 0) return std::nullopt;

    sockaddr_in addr{};
    socklen_t addr_len = sizeof(addr);
    if (getpeername(fd_, reinterpret_cast<struct sockaddr*>(&addr), &addr_len) == 0) {
        return SocketAddr::from_sock_addr(addr);
    }
    return std::nullopt;
}

bool TCPSocket::SetNonBlocking() {
    if (fd_ < 0) return false;
    int flags = fcntl(fd_, F_GETFL, 0);
    return fcntl(fd_, F_SETFL, flags | O_NONBLOCK) == 0;
}

bool TCPSocket::SetReuseAddr() {
    if (fd_ < 0) return false;
    int opt = 1;
    return setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == 0;
}

// SocketManager implementation
SocketManager::SocketManager() : epoll_fd_(-1), running_(false) {
#ifdef __linux__
    epoll_fd_ = epoll_create1(0);
#elif defined(__APPLE__) || defined(__FreeBSD__)
    epoll_fd_ = kqueue();
#endif
}

SocketManager::~SocketManager() {
    stop();
    if (epoll_fd_ >= 0) {
        close(epoll_fd_);
    }
}

bool SocketManager::register_socket(int fd, SocketEventCallback callback, uint32_t events) {
    if (epoll_fd_ < 0 || fd < 0) return false;

#ifdef __linux__
    epoll_event ev{};
    ev.events = events | EPOLLET;  // Edge-triggered
    ev.data.fd = fd;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == 0) {
        callbacks_[fd] = std::move(callback);
        return true;
    }
#elif defined(__APPLE__) || defined(__FreeBSD__)
    struct kevent ev[2];
    int n = 0;

    if (events & EPOLLIN) {
        EV_SET(&ev[n++], fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);
    }
    if (events & EPOLLOUT) {
        EV_SET(&ev[n++], fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, nullptr);
    }

    if (kevent(epoll_fd_, ev, n, nullptr, 0, nullptr) == 0) {
        callbacks_[fd] = std::move(callback);
        return true;
    }
#endif

    return false;
}

bool SocketManager::unregister(int fd) {
    if (epoll_fd_ < 0 || fd < 0) return false;

    callbacks_.erase(fd);

#ifdef __linux__
    return epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) == 0;
#elif defined(__APPLE__) || defined(__FreeBSD__)
    struct kevent ev[2];
    EV_SET(&ev[0], fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
    EV_SET(&ev[1], fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
    kevent(epoll_fd_, ev, 2, nullptr, 0, nullptr);
    return true;
#endif
}

bool SocketManager::modify(int fd, uint32_t events) {
    if (epoll_fd_ < 0 || fd < 0) return false;

#ifdef __linux__
    epoll_event ev{};
    ev.events = events | EPOLLET;
    ev.data.fd = fd;

    return epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) == 0;
#elif defined(__APPLE__) || defined(__FreeBSD__)
    // For kqueue, we need to delete and re-add
    unregister(fd);
    auto it = callbacks_.find(fd);
    if (it != callbacks_.end()) {
        auto callback = it->second;
        return register_socket(fd, callback, events);
    }
    return false;
#endif
}

int SocketManager::poll(int timeout_ms) {
    if (epoll_fd_ < 0) return -1;

    running_ = true;

#ifdef __linux__
    epoll_event events[64];
    int n = epoll_wait(epoll_fd_, events, 64, timeout_ms);

    for (int i = 0; i < n && running_; i++) {
        int fd = events[i].data.fd;
        auto it = callbacks_.find(fd);
        if (it == callbacks_.end()) continue;

        uint32_t ev = events[i].events;

        if (ev & EPOLLIN) {
            it->second(fd, SocketEvent::READABLE);
        }
        if (ev & EPOLLOUT) {
            it->second(fd, SocketEvent::WRITABLE);
        }
        if (ev & (EPOLLERR | EPOLLHUP)) {
            it->second(fd, SocketEvent::ERROR);
        }
    }
#elif defined(__APPLE__) || defined(__FreeBSD__)
    struct kevent events[64];
    struct timespec timeout;
    struct timespec* timeout_ptr = nullptr;

    if (timeout_ms >= 0) {
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_nsec = (timeout_ms % 1000) * 1000000;
        timeout_ptr = &timeout;
    }

    int n = kevent(epoll_fd_, nullptr, 0, events, 64, timeout_ptr);

    for (int i = 0; i < n && running_; i++) {
        int fd = static_cast<int>(events[i].ident);
        auto it = callbacks_.find(fd);
        if (it == callbacks_.end()) continue;

        if (events[i].filter == EVFILT_READ) {
            it->second(fd, SocketEvent::READABLE);
        } else if (events[i].filter == EVFILT_WRITE) {
            it->second(fd, SocketEvent::WRITABLE);
        }

        if (events[i].flags & EV_ERROR) {
            it->second(fd, SocketEvent::ERROR);
        }
    }
#endif

    return n;
}

void SocketManager::stop() {
    running_ = false;
}

}  // namespace net
}  // namespace p2p