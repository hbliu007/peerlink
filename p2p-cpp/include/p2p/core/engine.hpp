#pragma once

#include "types.hpp"
#include "connection.hpp"
#include "event.hpp"
#include <memory>
#include <string>

namespace p2p {

// Protocol handler interface
class ProtocolHandler {
public:
    virtual ~ProtocolHandler() = default;

    // Get protocol ID (e.g., "/ipfs/ping/1.0.0")
    virtual std::string GetProtocolId() const = 0;

    // Handle incoming connection
    virtual void HandleConnection(std::shared_ptr<Connection> conn) = 0;
};

// Engine - main entry point
class Engine {
public:
    virtual ~Engine() = default;

    // Create engine instance
    static std::unique_ptr<Engine> Create(const Config& config);

    // Start engine
    virtual Status Start() = 0;

    // Stop engine
    virtual Status Stop() = 0;

    // Connect to peer
    virtual Status Connect(const PeerId& peer_id,
                          std::shared_ptr<Connection>* conn) = 0;

    // Listen on address
    virtual Status Listen(const Multiaddr& addr,
                         std::shared_ptr<Listener>* listener) = 0;

    // Register protocol handler
    virtual Status RegisterProtocol(const std::string& protocol_id,
                                   std::unique_ptr<ProtocolHandler> handler) = 0;

    // Get event bus
    virtual EventBus& GetEventBus() = 0;

    // Get all connections
    virtual std::vector<std::shared_ptr<Connection>> GetConnections() = 0;

    // Get connection by peer ID
    virtual std::shared_ptr<Connection> GetConnection(const PeerId& peer_id) = 0;
};

} // namespace p2p