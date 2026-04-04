#include <gtest/gtest.h>
#include "p2p_test_framework.hpp"
#include <thread>
#include <chrono>

using namespace p2p::testing;

// P2P Connection Acceptance Tests
class P2PConnectionTest : public ::testing::Test {
protected:
    void SetUp() override {
        framework_ = std::make_unique<P2PTestFramework>();
        framework_->SetUp();

        // Wait for servers to be ready
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    void TearDown() override {
        framework_->TearDown();
    }

    std::unique_ptr<P2PTestFramework> framework_;
};

TEST_F(P2PConnectionTest, DirectConnection_FullConeNAT) {
    // Setup: Two clients behind Full Cone NAT
    auto client_a = framework_->CreateClient("client-a");
    auto client_b = framework_->CreateClient("client-b");

    framework_->SetNATType(client_a, NATType::FULL_CONE);
    framework_->SetNATType(client_b, NATType::FULL_CONE);

    // Execute: Establish P2P connection
    ASSERT_TRUE(client_a->ConnectToSignaling("127.0.0.1", 8080));
    ASSERT_TRUE(client_b->ConnectToSignaling("127.0.0.1", 8080));

    // Initiate connection from A to B
    auto connection = client_a->InitiateP2PConnection("client-b");
    ASSERT_NE(connection, nullptr) << "Failed to initiate P2P connection";

    // Verify: Connection established
    EXPECT_TRUE(connection->IsEstablished());
    EXPECT_EQ(connection->GetType(), ConnectionType::DIRECT);
    EXPECT_FALSE(connection->IsRelayed());

    // Verify: Data transfer works
    const std::string test_data = "Hello from client-a!";
    ASSERT_TRUE(connection->Send(test_data));

    auto received = client_b->ReceiveData(5000);
    EXPECT_EQ(received, test_data);
}

TEST_F(P2PConnectionTest, RelayFallback_SymmetricNAT) {
    // Setup: Two clients behind Symmetric NAT (direct connection impossible)
    auto client_a = framework_->CreateClient("client-a");
    auto client_b = framework_->CreateClient("client-b");

    framework_->SetNATType(client_a, NATType::SYMMETRIC);
    framework_->SetNATType(client_b, NATType::SYMMETRIC);

    // Execute: Establish connection (should fallback to relay)
    ASSERT_TRUE(client_a->ConnectToSignaling("127.0.0.1", 8080));
    ASSERT_TRUE(client_b->ConnectToSignaling("127.0.0.1", 8080));

    auto connection = client_a->InitiateP2PConnection("client-b");
    ASSERT_NE(connection, nullptr);

    // Verify: Connection uses relay
    EXPECT_TRUE(connection->IsEstablished());
    EXPECT_EQ(connection->GetType(), ConnectionType::RELAYED);
    EXPECT_TRUE(connection->IsRelayed());

    // Verify: Data transfer works through relay
    const std::string test_data = "Hello through relay!";
    ASSERT_TRUE(connection->Send(test_data));

    auto received = client_b->ReceiveData(5000);
    EXPECT_EQ(received, test_data);
}

TEST_F(P2PConnectionTest, DCUtR_Upgrade_RelayToDirect) {
    // Setup: Two clients that can do hole punching
    auto client_a = framework_->CreateClient("client-a");
    auto client_b = framework_->CreateClient("client-b");

    framework_->SetNATType(client_a, NATType::PORT_RESTRICTED);
    framework_->SetNATType(client_b, NATType::PORT_RESTRICTED);

    // Execute: Establish connection (initially via relay)
    ASSERT_TRUE(client_a->ConnectToSignaling("127.0.0.1", 8080));
    ASSERT_TRUE(client_b->ConnectToSignaling("127.0.0.1", 8080));

    auto connection = client_a->InitiateP2PConnection("client-b");
    ASSERT_NE(connection, nullptr);

    // Verify: Initially relayed
    EXPECT_TRUE(connection->IsEstablished());
    EXPECT_TRUE(connection->IsRelayed());

    // Execute: DCUtR upgrade
    ASSERT_TRUE(connection->AttemptDCUtRUpgrade());

    // Wait for upgrade to complete
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Verify: Upgraded to direct connection
    EXPECT_FALSE(connection->IsRelayed());
    EXPECT_EQ(connection->GetType(), ConnectionType::DIRECT);

    // Verify: Data transfer still works
    const std::string test_data = "Hello after upgrade!";
    ASSERT_TRUE(connection->Send(test_data));

    auto received = client_b->ReceiveData(5000);
    EXPECT_EQ(received, test_data);
}

TEST_F(P2PConnectionTest, ConnectionFailure_PeerOffline) {
    // Setup: One client, peer is offline
    auto client_a = framework_->CreateClient("client-a");

    ASSERT_TRUE(client_a->ConnectToSignaling("127.0.0.1", 8080));

    // Execute: Try to connect to offline peer
    auto connection = client_a->InitiateP2PConnection("offline-peer");

    // Verify: Connection fails gracefully
    EXPECT_EQ(connection, nullptr);
}

TEST_F(P2PConnectionTest, ConcurrentConnections_MultipleClients) {
    // Setup: Three clients
    auto client_a = framework_->CreateClient("client-a");
    auto client_b = framework_->CreateClient("client-b");
    auto client_c = framework_->CreateClient("client-c");

    framework_->SetNATType(client_a, NATType::FULL_CONE);
    framework_->SetNATType(client_b, NATType::FULL_CONE);
    framework_->SetNATType(client_c, NATType::FULL_CONE);

    // Execute: All clients connect to signaling
    ASSERT_TRUE(client_a->ConnectToSignaling("127.0.0.1", 8080));
    ASSERT_TRUE(client_b->ConnectToSignaling("127.0.0.1", 8080));
    ASSERT_TRUE(client_c->ConnectToSignaling("127.0.0.1", 8080));

    // Execute: A connects to both B and C
    auto conn_ab = client_a->InitiateP2PConnection("client-b");
    auto conn_ac = client_a->InitiateP2PConnection("client-c");

    // Verify: Both connections established
    ASSERT_NE(conn_ab, nullptr);
    ASSERT_NE(conn_ac, nullptr);
    EXPECT_TRUE(conn_ab->IsEstablished());
    EXPECT_TRUE(conn_ac->IsEstablished());

    // Verify: Independent data transfer
    ASSERT_TRUE(conn_ab->Send("Message to B"));
    ASSERT_TRUE(conn_ac->Send("Message to C"));

    EXPECT_EQ(client_b->ReceiveData(5000), "Message to B");
    EXPECT_EQ(client_c->ReceiveData(5000), "Message to C");
}

TEST_F(P2PConnectionTest, NetworkConditions_PacketLoss) {
    // Setup: Two clients with packet loss
    auto client_a = framework_->CreateClient("client-a");
    auto client_b = framework_->CreateClient("client-b");

    framework_->SetNATType(client_a, NATType::FULL_CONE);
    framework_->SetNATType(client_b, NATType::FULL_CONE);

    // Simulate 10% packet loss
    framework_->SetPacketLoss(0.10);

    // Execute: Establish connection
    ASSERT_TRUE(client_a->ConnectToSignaling("127.0.0.1", 8080));
    ASSERT_TRUE(client_b->ConnectToSignaling("127.0.0.1", 8080));

    auto connection = client_a->InitiateP2PConnection("client-b");

    // Verify: Connection still works despite packet loss
    ASSERT_NE(connection, nullptr);
    EXPECT_TRUE(connection->IsEstablished());

    // Verify: Data transfer with retries
    const std::string test_data = "Resilient message!";
    ASSERT_TRUE(connection->Send(test_data));

    auto received = client_b->ReceiveData(10000); // Longer timeout
    EXPECT_EQ(received, test_data);
}

TEST_F(P2PConnectionTest, NetworkConditions_HighLatency) {
    // Setup: Two clients with high latency
    auto client_a = framework_->CreateClient("client-a");
    auto client_b = framework_->CreateClient("client-b");

    framework_->SetNATType(client_a, NATType::FULL_CONE);
    framework_->SetNATType(client_b, NATType::FULL_CONE);

    // Simulate 200ms latency
    framework_->SetLatency(std::chrono::milliseconds(200));

    // Execute: Establish connection
    ASSERT_TRUE(client_a->ConnectToSignaling("127.0.0.1", 8080));
    ASSERT_TRUE(client_b->ConnectToSignaling("127.0.0.1", 8080));

    auto start = std::chrono::steady_clock::now();
    auto connection = client_a->InitiateP2PConnection("client-b");
    auto duration = std::chrono::steady_clock::now() - start;

    // Verify: Connection established (takes longer due to latency)
    ASSERT_NE(connection, nullptr);
    EXPECT_TRUE(connection->IsEstablished());
    EXPECT_GT(duration, std::chrono::milliseconds(400)); // At least 2 RTTs
}
