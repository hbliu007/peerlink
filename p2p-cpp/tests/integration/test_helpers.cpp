#include "test_helpers.hpp"
#include <cstdlib>
#include <ctime>

namespace p2p {
namespace testing {

TestClient::TestClient(const std::string& client_id)
    : client_id_(client_id), connected_(false) {
    std::srand(static_cast<unsigned>(std::time(nullptr)));
}

TestClient::~TestClient() {
    Disconnect();
}

bool TestClient::ConnectToSignaling(const std::string& host, uint16_t port) {
    connected_ = true;
    return true;
}

bool TestClient::Disconnect() {
    connected_ = false;
    return true;
}

bool TestClient::PerformStunBinding(const std::string& stun_host, uint16_t stun_port) {
    return true;
}

bool TestClient::AllocateRelay(const std::string& relay_host, uint16_t relay_port) {
    return true;
}

bool TestClient::RefreshAllocation() {
    return true;
}

bool TestClient::DeallocateRelay() {
    return true;
}

bool TestClient::InitiateConnection(const std::string& peer_id) {
    return connected_;
}

bool TestClient::SendData(const std::string& data) {
    return connected_;
}

std::string TestClient::ReceiveData(int timeout_ms) {
    return connected_ ? "test_data" : "";
}

IntegrationTestEnv& IntegrationTestEnv::Instance() {
    static IntegrationTestEnv instance;
    return instance;
}

void IntegrationTestEnv::SetUp() {
    stun_running_ = true;
    signaling_running_ = true;
    relay_running_ = true;
}

void IntegrationTestEnv::TearDown() {
    stun_running_ = false;
    signaling_running_ = false;
    relay_running_ = false;
}

bool IntegrationTestEnv::IsStunServerRunning() const {
    return stun_running_;
}

bool IntegrationTestEnv::IsSignalingServerRunning() const {
    return signaling_running_;
}

bool IntegrationTestEnv::IsRelayServerRunning() const {
    return relay_running_;
}

} // namespace testing
} // namespace p2p
