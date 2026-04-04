#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <optional>
#include <system_error>

namespace p2p {

// Forward declarations
class Connection;
class EventBus;
class ProtocolHandler;
class Transport;
class SecureTransport;

// Configuration
struct Config {
    std::string signaling_url;
    std::string stun_server;
    std::vector<std::string> relay_servers;
    uint16_t listen_port = 0;
    size_t max_connections = 1000;
    bool enable_relay = true;
    bool enable_upnp = false;
};

// Status codes
enum class StatusCode {
    OK = 0,
    ERROR_INVALID_ARGUMENT,
    ERROR_CONNECTION_FAILED,
    ERROR_TIMEOUT,
    ERROR_NOT_FOUND,
    ERROR_ALREADY_EXISTS,
    ERROR_PERMISSION_DENIED,
    ERROR_INTERNAL
};

// Status wrapper
class Status {
public:
    Status() : code_(StatusCode::OK) {}
    explicit Status(StatusCode code, std::string message = "")
        : code_(code), message_(std::move(message)) {}

    bool ok() const { return code_ == StatusCode::OK; }
    StatusCode code() const { return code_; }
    const std::string& message() const { return message_; }

    static Status OK() { return Status(); }
    static Status Error(StatusCode code, std::string message) {
        return Status(code, std::move(message));
    }

private:
    StatusCode code_;
    std::string message_;
};

// Conversion helpers between Status and std::error_code
inline std::error_code to_error_code(const Status& s) {
    if (s.ok()) return {};
    switch (s.code()) {
        case StatusCode::ERROR_INVALID_ARGUMENT:
            return std::make_error_code(std::errc::invalid_argument);
        case StatusCode::ERROR_CONNECTION_FAILED:
            return std::make_error_code(std::errc::connection_refused);
        case StatusCode::ERROR_TIMEOUT:
            return std::make_error_code(std::errc::timed_out);
        case StatusCode::ERROR_NOT_FOUND:
            return std::make_error_code(std::errc::no_such_file_or_directory);
        case StatusCode::ERROR_ALREADY_EXISTS:
            return std::make_error_code(std::errc::already_connected);
        case StatusCode::ERROR_PERMISSION_DENIED:
            return std::make_error_code(std::errc::permission_denied);
        case StatusCode::ERROR_INTERNAL:
        default:
            return std::make_error_code(std::errc::io_error);
    }
}

inline Status to_status(const std::error_code& ec) {
    if (!ec) return Status::OK();
    auto v = ec.value();
    if (v == static_cast<int>(std::errc::invalid_argument))
        return Status::Error(StatusCode::ERROR_INVALID_ARGUMENT, ec.message());
    if (v == static_cast<int>(std::errc::connection_refused) ||
        v == static_cast<int>(std::errc::network_unreachable) ||
        v == static_cast<int>(std::errc::not_connected))
        return Status::Error(StatusCode::ERROR_CONNECTION_FAILED, ec.message());
    if (v == static_cast<int>(std::errc::timed_out))
        return Status::Error(StatusCode::ERROR_TIMEOUT, ec.message());
    if (v == static_cast<int>(std::errc::already_connected))
        return Status::Error(StatusCode::ERROR_ALREADY_EXISTS, ec.message());
    if (v == static_cast<int>(std::errc::permission_denied))
        return Status::Error(StatusCode::ERROR_PERMISSION_DENIED, ec.message());
    return Status::Error(StatusCode::ERROR_INTERNAL, ec.message());
}

// Peer ID
class PeerId {
public:
    explicit PeerId(std::string id) : id_(std::move(id)) {}

    const std::string& ToString() const { return id_; }
    bool operator==(const PeerId& other) const { return id_ == other.id_; }
    bool operator!=(const PeerId& other) const { return !(*this == other); }

private:
    std::string id_;
};

// Multiaddr (libp2p addressing)
class Multiaddr {
public:
    explicit Multiaddr(std::string addr) : addr_(std::move(addr)), parsed_(false) {}

    const std::string& ToString() const { return addr_; }

    /**
     * Parse multiaddr string and validate format
     * @return Status OK if valid, error otherwise
     */
    Status Parse();

    /**
     * Serialize multiaddr to bytes (varint-encoded protocol codes + addresses)
     * @return Serialized bytes, empty if not parsed
     */
    std::vector<uint8_t> ToBytes() const;

    /**
     * Deserialize multiaddr from bytes
     * @param bytes Serialized multiaddr
     * @return Multiaddr instance or nullopt on error
     */
    static std::optional<Multiaddr> FromBytes(const std::vector<uint8_t>& bytes);

private:
    std::string addr_;
    bool parsed_;
    std::vector<uint8_t> bytes_;  // Cached serialized form
};

// Connection ID
using ConnectionId = uint64_t;

} // namespace p2p