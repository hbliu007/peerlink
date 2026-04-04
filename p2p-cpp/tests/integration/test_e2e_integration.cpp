#include <gtest/gtest.h>
#include "test_helpers.hpp"
#include <thread>
#include <chrono>

using namespace p2p::testing;

class E2EIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        env_ = &IntegrationTestEnv::Instance();
        env_->SetUp();

        // Wait for servers to be ready
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    void TearDown() override {
        env_->TearDown();
    }

    IntegrationTestEnv* env_;
};

TEST_F(E2EIntegrationTest, ServerStartup) {
    EXPECT_TRUE(env_->IsStunServerRunning());
    EXPECT_TRUE(env_->IsSignalingServerRunning());
    EXPECT_TRUE(env_->IsRelayServerRunning());
}

TEST_F(E2EIntegrationTest, SingleClientConnection) {
    TestClient client("client1");

    EXPECT_TRUE(client.ConnectToSignaling("127.0.0.1", env_->GetSignalingPort()));
    EXPECT_TRUE(client.IsConnected());

    EXPECT_TRUE(client.Disconnect());
    EXPECT_FALSE(client.IsConnected());
}

TEST_F(E2EIntegrationTest, StunBinding) {
    TestClient client("client2");

    EXPECT_TRUE(client.ConnectToSignaling("127.0.0.1", env_->GetSignalingPort()));
    EXPECT_TRUE(client.PerformStunBinding("127.0.0.1", env_->GetStunPort()));
}

TEST_F(E2EIntegrationTest, TurnAllocation) {
    TestClient client("client3");

    EXPECT_TRUE(client.ConnectToSignaling("127.0.0.1", env_->GetSignalingPort()));
    EXPECT_TRUE(client.AllocateRelay("127.0.0.1", env_->GetRelayPort()));
    EXPECT_TRUE(client.RefreshAllocation());
    EXPECT_TRUE(client.DeallocateRelay());
}

TEST_F(E2EIntegrationTest, ConnectionRecovery) {
    TestClient client("client8");

    EXPECT_TRUE(client.ConnectToSignaling("127.0.0.1", env_->GetSignalingPort()));
    EXPECT_TRUE(client.Disconnect());

    // Reconnect
    EXPECT_TRUE(client.ConnectToSignaling("127.0.0.1", env_->GetSignalingPort()));
    EXPECT_TRUE(client.IsConnected());
}
