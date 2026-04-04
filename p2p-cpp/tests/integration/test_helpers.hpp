#pragma once

#include <memory>
#include <thread>
#include <chrono>
#include <string>

namespace p2p {
namespace testing {

// Test server instance wrapper
template<typename ServerType>
class TestServer {
public:
    TestServer(const typename ServerType::Config& config)
        : config_(config), running_(false) {}

    ~TestServer() {
        Stop();
    }

    void Start() {
        if (running_) return;

        server_ = std::make_unique<ServerType>(config_);
        running_ = true;

        server_thread_ = std::thread([this]() {
            server_->Run();
        });

        // Wait for server to start
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    void Stop() {
        if (!running_) return;

        if (server_) {
            server_->Stop();
        }

        if (server_thread_.joinable()) {
            server_thread_.join();
        }

        running_ = false;
    }

    bool IsRunning() const { return running_; }

private:
    typename ServerType::Config config_;
    std::unique_ptr<ServerType> server_;
    std::thread server_thread_;
    bool running_;
};

// Test client for integration testing
class TestClient {
public:
    TestClient(const std::string& client_id);
    ~TestClient();

    // Connection
    bool ConnectToSignaling(const std::string& host, uint16_t port);
    bool Disconnect();

    // STUN
    bool PerformStunBinding(const std::string& stun_host, uint16_t stun_port);

    // TURN
    bool AllocateRelay(const std::string& relay_host, uint16_t relay_port);
    bool RefreshAllocation();
    bool DeallocateRelay();

    // P2P
    bool InitiateConnection(const std::string& peer_id);
    bool SendData(const std::string& data);
    std::string ReceiveData(int timeout_ms = 5000);

    // Status
    bool IsConnected() const { return connected_; }
    std::string GetClientId() const { return client_id_; }

private:
    std::string client_id_;
    bool connected_;
    // Add actual implementation members
};

// Integration test environment
class IntegrationTestEnv {
public:
    static IntegrationTestEnv& Instance();

    void SetUp();
    void TearDown();

    // Server access
    bool IsStunServerRunning() const;
    bool IsSignalingServerRunning() const;
    bool IsRelayServerRunning() const;

    // Ports
    uint16_t GetStunPort() const { return 3478; }
    uint16_t GetSignalingPort() const { return 8080; }
    uint16_t GetRelayPort() const { return 3479; }

private:
    IntegrationTestEnv() = default;
    ~IntegrationTestEnv() = default;

    bool stun_running_ = false;
    bool signaling_running_ = false;
    bool relay_running_ = false;
};

} // namespace testing
} // namespace p2p
