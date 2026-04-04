#include "p2p_test_framework.hpp"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <random>
#include <set>

namespace p2p {
namespace testing {

// Mock P2P Connection Implementation

class MockP2PConnectionImpl : public MockP2PConnection {
public:
    MockP2PConnectionImpl(const std::string& local_id, const std::string& remote_id,
                          ConnectionType type, MessageRouter* router)
        : local_id_(local_id)
        , remote_id_(remote_id)
        , type_(type)
        , established_(true)
        , router_(router)
        , conn_key_(local_id, remote_id) {

        // Register this endpoint with the router
        if (router_) {
            router_->RegisterEndpoint(conn_key_, local_id_, &receive_queue_,
                                      &receive_mutex_, &receive_cv_);
        }
    }

    ~MockP2PConnectionImpl() override {
        // Unregister from router
        if (router_) {
            router_->UnregisterEndpoint(conn_key_, local_id_);
        }
    }

    bool IsEstablished() const override { return established_; }
    ConnectionType GetType() const override { return type_; }
    bool IsRelayed() const override { return type_ == ConnectionType::RELAYED; }

    bool Send(const std::string& data) override {
        if (!established_) return false;

        // Route message through global router to remote peer
        if (router_) {
            return router_->Send(conn_key_, remote_id_, data);
        }
        return false;
    }

    std::string Receive(int timeout_ms) override {
        std::unique_lock<std::mutex> lock(receive_mutex_);

        // Wait for data with timeout
        if (timeout_ms > 0) {
            receive_cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                                [this] { return !receive_queue_.empty(); });
        }

        if (receive_queue_.empty()) {
            return "";
        }

        std::string data = receive_queue_.front();
        receive_queue_.pop();
        return data;
    }

    bool AttemptDCUtRUpgrade() override {
        if (type_ == ConnectionType::RELAYED) {
            // Simulate DCUtR upgrade
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            type_ = ConnectionType::DIRECT;
            return true;
        }
        return false;
    }

    std::string GetLocalAddress() const override { return "127.0.0.1:12345"; }
    std::string GetRemoteAddress() const override { return "127.0.0.1:54321"; }
    std::string GetRemotePeerId() const override { return remote_id_; }

private:
    std::string local_id_;
    std::string remote_id_;
    ConnectionType type_;
    bool established_;
    MessageRouter* router_;
    ConnectionKey conn_key_;

    // Receive queue (remote messages routed here)
    std::queue<std::string> receive_queue_;
    mutable std::mutex receive_mutex_;
    std::condition_variable receive_cv_;
};

// Mock Test Client Implementation

class MockTestClientImpl : public MockTestClient {
public:
    MockTestClientImpl(const std::string& client_id, MessageRouter* router,
                      IClientConnectionCallback* framework)
        : client_id_(client_id)
        , connected_(false)
        , nat_type_(NATType::NONE)
        , router_(router)
        , framework_(framework) {}

    bool ConnectToSignaling(const std::string& host, uint16_t port) override {
        connected_ = true;
        return true;
    }

    bool Disconnect() override {
        connected_ = false;
        return true;
    }

    std::shared_ptr<MockP2PConnection> InitiateP2PConnection(
        const std::string& peer_id) override {
        if (!connected_) return nullptr;

        // Check if peer exists in the framework
        auto peer_client = framework_->GetClient(peer_id);
        if (!peer_client) {
            return nullptr;  // Peer doesn't exist
        }

        // Simulate network latency for connection establishment
        auto network_latency = framework_->GetNetworkLatency();
        if (network_latency.count() > 0) {
            std::this_thread::sleep_for(network_latency * 2);  // Round trip
        }

        // Determine connection type based on NAT types
        ConnectionType conn_type = DetermineConnectionType(peer_id);

        // Create the initiator's connection
        auto connection = std::make_shared<MockP2PConnectionImpl>(
            client_id_, peer_id, conn_type, router_);

        connections_[peer_id] = connection;

        // Establish bidirectional connection through framework
        // This creates the receiving endpoint on the peer side
        framework_->EstablishBidirectionalConnection(client_id_, peer_id, conn_type);

        return connection;
    }

    /**
     * @brief Accept an incoming connection (called by framework when peer connects)
     * This creates the receiving side endpoint
     */
    std::shared_ptr<MockP2PConnection> AcceptIncomingConnection(
        const std::string& peer_id, ConnectionType type) override {
        // Create receive-side connection
        auto connection = std::make_shared<MockP2PConnectionImpl>(
            client_id_, peer_id, type, router_);

        connections_[peer_id] = connection;
        return connection;
    }

    std::string ReceiveData(int timeout_ms) override {
        auto start = std::chrono::steady_clock::now();
        auto timeout = std::chrono::milliseconds(timeout_ms);

        // Poll all connections for data
        while (true) {
            for (auto& [peer_id, conn] : connections_) {
                std::string data = conn->Receive(0);  // Non-blocking check
                if (!data.empty()) {
                    return data;
                }
            }

            // Check timeout
            if (timeout_ms > 0) {
                auto elapsed = std::chrono::steady_clock::now() - start;
                if (elapsed >= timeout) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } else {
                break;
            }
        }
        return "";
    }

    std::string GetClientId() const override { return client_id_; }
    bool IsConnected() const override { return connected_; }

    void SetNATType(NATType type) { nat_type_ = type; }
    NATType GetNATType() const { return nat_type_; }

private:
    ConnectionType DetermineConnectionType(const std::string& /* peer_id */) {
        // Simplified NAT traversal logic
        if (nat_type_ == NATType::SYMMETRIC) {
            return ConnectionType::RELAYED;
        }
        if (nat_type_ == NATType::FULL_CONE || nat_type_ == NATType::NONE) {
            return ConnectionType::DIRECT;
        }
        // Port restricted can do hole punching
        return ConnectionType::RELAYED; // Initially relayed, can upgrade
    }

    std::string client_id_;
    bool connected_;
    NATType nat_type_;
    MessageRouter* router_;
    IClientConnectionCallback* framework_;
    std::map<std::string, std::shared_ptr<MockP2PConnection>> connections_;
};

// Mock Server Implementations

class MockStunServerImpl : public MockStunServer {
public:
    void Start(uint16_t port) override { running_ = true; }
    void Stop() override { running_ = false; }
    bool IsRunning() const override { return running_; }

    void SetResponseDelay(std::chrono::milliseconds delay) override {
        response_delay_ = delay;
    }
    void SetFailureRate(double rate) override { failure_rate_ = rate; }

private:
    bool running_ = false;
    std::chrono::milliseconds response_delay_{0};
    double failure_rate_ = 0.0;
};

class MockRelayServerImpl : public MockRelayServer {
public:
    void Start(uint16_t port) override { running_ = true; }
    void Stop() override { running_ = false; }
    bool IsRunning() const override { return running_; }

    void SetBandwidthLimit(size_t bytes_per_second) override {
        bandwidth_limit_ = bytes_per_second;
    }
    void SetMaxAllocations(size_t max) override { max_allocations_ = max; }

private:
    bool running_ = false;
    size_t bandwidth_limit_ = 0;
    size_t max_allocations_ = 1000;
};

class MockSignalingServerImpl : public MockSignalingServer {
public:
    void Start(uint16_t port) override { running_ = true; }
    void Stop() override { running_ = false; }
    bool IsRunning() const override { return running_; }

    size_t GetConnectedClientCount() const override {
        return connected_clients_.size();
    }

    bool IsClientConnected(const std::string& client_id) const override {
        return connected_clients_.find(client_id) != connected_clients_.end();
    }

    void RegisterClient(const std::string& client_id) {
        connected_clients_.insert(client_id);
    }

    void UnregisterClient(const std::string& client_id) {
        connected_clients_.erase(client_id);
    }

private:
    bool running_ = false;
    std::set<std::string> connected_clients_;
};

// Network Simulator Implementation

class NetworkSimulatorImpl : public NetworkSimulator {
public:
    void SetPacketLoss(double rate) override { packet_loss_ = rate; }
    void SetLatency(std::chrono::milliseconds latency) override {
        latency_ = latency;
    }
    void SetJitter(std::chrono::milliseconds jitter) override {
        jitter_ = jitter;
    }
    void SetBandwidth(size_t bytes_per_second) override {
        bandwidth_ = bytes_per_second;
    }

    void Reset() override {
        packet_loss_ = 0.0;
        latency_ = std::chrono::milliseconds(0);
        jitter_ = std::chrono::milliseconds(0);
        bandwidth_ = 0;
    }

    std::chrono::milliseconds GetLatency() const override { return latency_; }

private:
    double packet_loss_ = 0.0;
    std::chrono::milliseconds latency_{0};
    std::chrono::milliseconds jitter_{0};
    size_t bandwidth_ = 0;
};

// NAT Simulator Implementation

class NATSimulatorImpl : public NATSimulator {
public:
    void SetNATType(const std::string& client_id, NATType type) override {
        nat_types_[client_id] = type;
    }

    NATType GetNATType(const std::string& client_id) const override {
        auto it = nat_types_.find(client_id);
        return (it != nat_types_.end()) ? it->second : NATType::NONE;
    }

    void SetMappingLifetime(std::chrono::seconds lifetime) override {
        mapping_lifetime_ = lifetime;
    }

    void SetPortAllocationStrategy(const std::string& strategy) override {
        port_strategy_ = strategy;
    }

private:
    std::map<std::string, NATType> nat_types_;
    std::chrono::seconds mapping_lifetime_{300};
    std::string port_strategy_ = "sequential";
};

// P2P Test Framework Implementation

class P2PTestFramework::Impl : public IClientConnectionCallback {
public:
    Impl()
        : message_router_(std::make_unique<MessageRouter>())
        , network_sim_(std::make_shared<NetworkSimulatorImpl>()) {
        stun_server_ = std::make_shared<MockStunServerImpl>();
        relay_server_ = std::make_shared<MockRelayServerImpl>();
        signaling_server_ = std::make_shared<MockSignalingServerImpl>();
        nat_sim_ = std::make_shared<NATSimulatorImpl>();
    }

    // IClientConnectionCallback implementation
    std::shared_ptr<MockTestClient> GetClient(const std::string& client_id) override {
        auto it = clients_.find(client_id);
        if (it != clients_.end()) {
            return it->second;
        }
        return nullptr;
    }

    std::chrono::milliseconds GetNetworkLatency() const override {
        return network_sim_->GetLatency();
    }

    void EstablishBidirectionalConnection(const std::string& client_a_id,
                                          const std::string& client_b_id,
                                          ConnectionType type) override {
        auto client_a = GetClient(client_a_id);
        auto client_b = GetClient(client_b_id);

        if (!client_a || !client_b) {
            return;  // One or both clients don't exist
        }

        auto impl_b = std::static_pointer_cast<MockTestClientImpl>(client_b);

        // Create endpoint only on peer side (initiator already has it)
        impl_b->AcceptIncomingConnection(client_a_id, type);
    }

    void SetUp() {
        stun_server_->Start(3478);
        relay_server_->Start(3479);
        signaling_server_->Start(8080);
    }

    void TearDown() {
        clients_.clear();
        message_router_->Clear();
        stun_server_->Stop();
        relay_server_->Stop();
        signaling_server_->Stop();
        network_sim_->Reset();
    }

    std::shared_ptr<MockTestClient> CreateClient(const std::string& client_id) {
        auto client = std::make_shared<MockTestClientImpl>(
            client_id, message_router_.get(), this);
        clients_[client_id] = client;
        return client;
    }

    void SetNATType(std::shared_ptr<MockTestClient> client, NATType type) {
        auto impl = std::static_pointer_cast<MockTestClientImpl>(client);
        impl->SetNATType(type);
        nat_sim_->SetNATType(impl->GetClientId(), type);
    }

    NATType GetNATType(std::shared_ptr<MockTestClient> client) const {
        auto impl = std::static_pointer_cast<MockTestClientImpl>(client);
        return impl->GetNATType();
    }

    std::shared_ptr<MockStunServer> stun_server_;
    std::shared_ptr<MockRelayServer> relay_server_;
    std::shared_ptr<MockSignalingServer> signaling_server_;
    std::shared_ptr<NetworkSimulatorImpl> network_sim_;
    std::shared_ptr<NATSimulator> nat_sim_;
    std::unique_ptr<MessageRouter> message_router_;
    std::map<std::string, std::shared_ptr<MockTestClientImpl>> clients_;
};

// ============================================================================
// P2P Test Framework Public Interface
// ============================================================================

P2PTestFramework::P2PTestFramework()
    : impl_(std::make_unique<Impl>()) {}

P2PTestFramework::~P2PTestFramework() = default;

void P2PTestFramework::SetUp() {
    impl_->SetUp();
}

void P2PTestFramework::TearDown() {
    impl_->TearDown();
}

std::shared_ptr<MockTestClient> P2PTestFramework::CreateClient(
    const std::string& client_id) {
    return impl_->CreateClient(client_id);
}

std::shared_ptr<MockTestClient> P2PTestFramework::GetClient(
    const std::string& client_id) {
    return impl_->GetClient(client_id);
}

void P2PTestFramework::RemoveClient(const std::string& client_id) {
    impl_->clients_.erase(client_id);
}

std::shared_ptr<MockStunServer> P2PTestFramework::GetStunServer() {
    return impl_->stun_server_;
}

std::shared_ptr<MockRelayServer> P2PTestFramework::GetRelayServer() {
    return impl_->relay_server_;
}

std::shared_ptr<MockSignalingServer> P2PTestFramework::GetSignalingServer() {
    return impl_->signaling_server_;
}

void P2PTestFramework::SetPacketLoss(double rate) {
    impl_->network_sim_->SetPacketLoss(rate);
}

void P2PTestFramework::SetLatency(std::chrono::milliseconds latency) {
    impl_->network_sim_->SetLatency(latency);
}

void P2PTestFramework::SetJitter(std::chrono::milliseconds jitter) {
    impl_->network_sim_->SetJitter(jitter);
}

void P2PTestFramework::SetBandwidth(size_t bytes_per_second) {
    impl_->network_sim_->SetBandwidth(bytes_per_second);
}

void P2PTestFramework::SetNATType(std::shared_ptr<MockTestClient> client,
                                  NATType type) {
    impl_->SetNATType(client, type);
}

NATType P2PTestFramework::GetNATType(std::shared_ptr<MockTestClient> client) const {
    return impl_->GetNATType(client);
}

void P2PTestFramework::WaitForCondition(std::function<bool()> condition,
                                        std::chrono::milliseconds timeout) {
    WaitFor(condition, timeout);
}

void P2PTestFramework::SimulateNetworkPartition(const std::string& /* client_a */,
                                                const std::string& /* client_b */) {
    // TODO: Implement network partition simulation
}

void P2PTestFramework::RestoreNetwork() {
    impl_->network_sim_->Reset();
}

} // namespace testing
} // namespace p2p
