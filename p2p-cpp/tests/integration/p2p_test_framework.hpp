#pragma once

#include <memory>
#include <string>
#include <map>
#include <chrono>
#include <thread>
#include <functional>
#include <queue>
#include <mutex>

namespace p2p {
namespace testing {

// Forward declarations
class MessageRouter;

// Connection Pair Key

/**
 * @brief Unique key for a bidirectional connection pair
 * Ensures consistent ordering regardless of initiation direction
 */
struct ConnectionKey {
    std::string peer_a;
    std::string peer_b;

    ConnectionKey(const std::string& a, const std::string& b) {
        if (a < b) {
            peer_a = a;
            peer_b = b;
        } else {
            peer_a = b;
            peer_b = a;
        }
    }

    bool operator<(const ConnectionKey& other) const {
        if (peer_a != other.peer_a) return peer_a < other.peer_a;
        return peer_b < other.peer_b;
    }
};

// Message Router (Global Message Exchange)

/**
 * @brief Global message router for cross-connection communication
 * Routes messages between clients by tracking connection pairs
 */
class MessageRouter {
public:
    /**
     * @brief Register a connection endpoint
     * @param key Connection pair identifier
     * @param local_id Local client ID
     * @param queue Pointer to the client's receive queue
     */
    void RegisterEndpoint(const ConnectionKey& key,
                          const std::string& local_id,
                          std::queue<std::string>* queue,
                          std::mutex* mutex,
                          std::condition_variable* cv = nullptr) {
        std::lock_guard<std::mutex> lock(router_mutex_);
        std::string endpoint_key = local_id + "@" + key.peer_a + "-" + key.peer_b;
        endpoints_[endpoint_key] = {queue, mutex, cv};
    }

    /**
     * @brief Unregister a connection endpoint
     */
    void UnregisterEndpoint(const ConnectionKey& key, const std::string& local_id) {
        std::lock_guard<std::mutex> lock(router_mutex_);
        std::string endpoint_key = local_id + "@" + key.peer_a + "-" + key.peer_b;
        endpoints_.erase(endpoint_key);
    }

    /**
     * @brief Send message to remote peer
     * @return true if message was delivered
     */
    bool Send(const ConnectionKey& key,
              const std::string& receiver_id,
              const std::string& data) {
        std::string receiver_key = receiver_id + "@" + key.peer_a + "-" + key.peer_b;

        std::lock_guard<std::mutex> lock(router_mutex_);
        auto it = endpoints_.find(receiver_key);
        if (it == endpoints_.end()) {
            return false;
        }

        // Lock the receiver's queue and add message
        std::lock_guard<std::mutex> queue_lock(*it->second.mutex);
        it->second.queue->push(data);

        // Notify receiver if they have a condition variable
        if (it->second.cv) {
            it->second.cv->notify_one();
        }

        return true;
    }

    /**
     * @brief Check if a peer endpoint exists
     */
    bool HasEndpoint(const ConnectionKey& key, const std::string& peer_id) {
        std::string endpoint_key = peer_id + "@" + key.peer_a + "-" + key.peer_b;
        std::lock_guard<std::mutex> lock(router_mutex_);
        return endpoints_.find(endpoint_key) != endpoints_.end();
    }

    void Clear() {
        std::lock_guard<std::mutex> lock(router_mutex_);
        endpoints_.clear();
    }

private:
    struct EndpointInfo {
        std::queue<std::string>* queue;
        std::mutex* mutex;
        std::condition_variable* cv;
    };

    std::mutex router_mutex_;
    std::map<std::string, EndpointInfo> endpoints_;
};

// NAT Type Enumeration

enum class NATType {
    NONE,              // No NAT (direct internet connection)
    FULL_CONE,         // Full Cone NAT (easiest to traverse)
    RESTRICTED_CONE,   // Restricted Cone NAT
    PORT_RESTRICTED,   // Port Restricted Cone NAT
    SYMMETRIC          // Symmetric NAT (hardest to traverse)
};

// Connection Type

enum class ConnectionType {
    DIRECT,    // Direct P2P connection
    RELAYED,   // Connection through relay server
    UNKNOWN
};

// Mock P2P Connection

class MockP2PConnection {
public:
    virtual ~MockP2PConnection() = default;

    // Connection status
    virtual bool IsEstablished() const = 0;
    virtual ConnectionType GetType() const = 0;
    virtual bool IsRelayed() const = 0;

    // Data transfer
    virtual bool Send(const std::string& data) = 0;
    virtual std::string Receive(int timeout_ms = 5000) = 0;

    // DCUtR upgrade
    virtual bool AttemptDCUtRUpgrade() = 0;

    // Connection info
    virtual std::string GetLocalAddress() const = 0;
    virtual std::string GetRemoteAddress() const = 0;
    virtual std::string GetRemotePeerId() const = 0;
};

// Mock Test Client

class MockTestClient {
public:
    virtual ~MockTestClient() = default;

    // Signaling
    virtual bool ConnectToSignaling(const std::string& host, uint16_t port) = 0;
    virtual bool Disconnect() = 0;

    // P2P Connection
    virtual std::shared_ptr<MockP2PConnection> InitiateP2PConnection(
        const std::string& peer_id) = 0;

    // Data reception
    virtual std::string ReceiveData(int timeout_ms = 5000) = 0;

    // Client info
    virtual std::string GetClientId() const = 0;
    virtual bool IsConnected() const = 0;

protected:
    // Internal method for accepting incoming connections (used by framework)
    virtual std::shared_ptr<MockP2PConnection> AcceptIncomingConnection(
        const std::string& peer_id, ConnectionType type) = 0;

    // Allow P2PTestFramework to call AcceptIncomingConnection
    friend class P2PTestFramework;
};

// Mock STUN Server

class MockStunServer {
public:
    virtual ~MockStunServer() = default;

    virtual void Start(uint16_t port) = 0;
    virtual void Stop() = 0;
    virtual bool IsRunning() const = 0;

    // Configure behavior
    virtual void SetResponseDelay(std::chrono::milliseconds delay) = 0;
    virtual void SetFailureRate(double rate) = 0; // 0.0 to 1.0
};

// ============================================================================
// Mock TURN/Relay Server
// ============================================================================

class MockRelayServer {
public:
    virtual ~MockRelayServer() = default;

    virtual void Start(uint16_t port) = 0;
    virtual void Stop() = 0;
    virtual bool IsRunning() const = 0;

    // Configure behavior
    virtual void SetBandwidthLimit(size_t bytes_per_second) = 0;
    virtual void SetMaxAllocations(size_t max) = 0;
};

// Mock Signaling Server

class MockSignalingServer {
public:
    virtual ~MockSignalingServer() = default;

    virtual void Start(uint16_t port) = 0;
    virtual void Stop() = 0;
    virtual bool IsRunning() const = 0;

    // Client management
    virtual size_t GetConnectedClientCount() const = 0;
    virtual bool IsClientConnected(const std::string& client_id) const = 0;
};

// Network Simulator

class NetworkSimulator {
public:
    virtual ~NetworkSimulator() = default;

    // Network conditions
    virtual void SetPacketLoss(double rate) = 0;  // 0.0 to 1.0
    virtual void SetLatency(std::chrono::milliseconds latency) = 0;
    virtual void SetJitter(std::chrono::milliseconds jitter) = 0;
    virtual void SetBandwidth(size_t bytes_per_second) = 0;

    // Reset to normal conditions
    virtual void Reset() = 0;

    // Get current latency (for testing)
    virtual std::chrono::milliseconds GetLatency() const = 0;
};

// NAT Simulator

class NATSimulator {
public:
    virtual ~NATSimulator() = default;

    // Configure NAT behavior for a client
    virtual void SetNATType(const std::string& client_id, NATType type) = 0;
    virtual NATType GetNATType(const std::string& client_id) const = 0;

    // NAT mapping behavior
    virtual void SetMappingLifetime(std::chrono::seconds lifetime) = 0;
    virtual void SetPortAllocationStrategy(const std::string& strategy) = 0;
};

// P2P Test Framework

/**
 * @brief Client connection callback interface
 * Used by clients to interact with the framework for connection setup
 */
class IClientConnectionCallback {
public:
    virtual ~IClientConnectionCallback() = default;
    virtual std::shared_ptr<MockTestClient> GetClient(const std::string& client_id) = 0;
    virtual std::chrono::milliseconds GetNetworkLatency() const = 0;
    virtual void EstablishBidirectionalConnection(
        const std::string& client_a_id,
        const std::string& client_b_id,
        ConnectionType type) = 0;
};

class P2PTestFramework {
public:
    P2PTestFramework();
    ~P2PTestFramework();

    // Setup and teardown
    void SetUp();
    void TearDown();

    // Client management
    std::shared_ptr<MockTestClient> CreateClient(const std::string& client_id);
    void RemoveClient(const std::string& client_id);

    // Server access
    std::shared_ptr<MockStunServer> GetStunServer();
    std::shared_ptr<MockRelayServer> GetRelayServer();
    std::shared_ptr<MockSignalingServer> GetSignalingServer();

    // Network simulation
    void SetPacketLoss(double rate);
    void SetLatency(std::chrono::milliseconds latency);
    void SetJitter(std::chrono::milliseconds jitter);
    void SetBandwidth(size_t bytes_per_second);

    // NAT simulation
    void SetNATType(std::shared_ptr<MockTestClient> client, NATType type);
    NATType GetNATType(std::shared_ptr<MockTestClient> client) const;

    // Test utilities
    void WaitForCondition(std::function<bool()> condition,
                         std::chrono::milliseconds timeout);
    void SimulateNetworkPartition(const std::string& client_a,
                                  const std::string& client_b);
    void RestoreNetwork();

    /**
     * @brief Get client by ID (internal use)
     */
    std::shared_ptr<MockTestClient> GetClient(const std::string& client_id);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    // Allow Impl to access callback interface
    friend class Impl;
};

// Test Utilities

/**
 * @brief Wait for a condition to become true
 */
inline bool WaitFor(std::function<bool()> condition,
                    std::chrono::milliseconds timeout) {
    auto start = std::chrono::steady_clock::now();
    while (!condition()) {
        if (std::chrono::steady_clock::now() - start > timeout) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return true;
}

/**
 * @brief Generate random test data
 */
inline std::string GenerateTestData(size_t size) {
    std::string data;
    data.reserve(size);
    for (size_t i = 0; i < size; ++i) {
        data += static_cast<char>('A' + (i % 26));
    }
    return data;
}

/**
 * @brief Measure operation latency
 */
template<typename Func>
std::chrono::milliseconds MeasureLatency(Func&& func) {
    auto start = std::chrono::steady_clock::now();
    func();
    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
}

} // namespace testing
} // namespace p2p
