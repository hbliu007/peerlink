#pragma once

#include "types.hpp"
#include <memory>
#include <span>
#include <functional>

namespace p2p {

// Transport-level connection state (distinct from core::ConnectionState in p2p_client.hpp)
enum class TransportState {
    CONNECTING,
    CONNECTED,
    DISCONNECTING,
    DISCONNECTED,
    ERROR
};

// Connection interface
class Connection {
public:
    virtual ~Connection() = default;

    // Get connection ID
    virtual ConnectionId GetId() const = 0;

    // Get peer ID
    virtual const PeerId& GetPeerId() const = 0;

    // Get connection state
    virtual TransportState GetState() const = 0;

    // Send data (zero-copy)
    virtual Status Send(std::span<const uint8_t> data) = 0;

    // Receive data (async callback)
    using ReceiveCallback = std::function<void(Status, std::span<const uint8_t>)>;
    virtual void ReceiveAsync(ReceiveCallback callback) = 0;

    // Close connection
    virtual Status Close() = 0;

    // Get local address
    virtual Multiaddr GetLocalAddr() const = 0;

    // Get remote address
    virtual Multiaddr GetRemoteAddr() const = 0;
};

// Listener interface
class Listener {
public:
    virtual ~Listener() = default;

    // Accept new connection
    using AcceptCallback = std::function<void(Status, std::shared_ptr<Connection>)>;
    virtual void AcceptAsync(AcceptCallback callback) = 0;

    // Get listen address
    virtual Multiaddr GetAddr() const = 0;

    // Close listener
    virtual Status Close() = 0;
};

} // namespace p2p