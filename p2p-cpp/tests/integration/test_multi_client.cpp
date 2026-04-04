#include <gtest/gtest.h>
#include "test_helpers.hpp"
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>

using namespace p2p::testing;

class MultiClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        env_ = &IntegrationTestEnv::Instance();
        env_->SetUp();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    void TearDown() override {
        env_->TearDown();
    }

    IntegrationTestEnv* env_;
};

TEST_F(MultiClientTest, TenConcurrentClients) {
    const int num_clients = 10;
    std::vector<std::unique_ptr<TestClient>> clients;
    std::atomic<int> success_count{0};

    // Create clients
    for (int i = 0; i < num_clients; ++i) {
        clients.push_back(std::make_unique<TestClient>("multi_client" + std::to_string(i)));
    }

    // Connect all clients concurrently
    std::vector<std::thread> threads;
    for (auto& client : clients) {
        threads.emplace_back([&client, &success_count, this]() {
            if (client->ConnectToSignaling("127.0.0.1", env_->GetSignalingPort())) {
                success_count++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), num_clients);
}

TEST_F(MultiClientTest, ConcurrentStunBindings) {
    const int num_clients = 20;
    std::vector<std::unique_ptr<TestClient>> clients;
    std::atomic<int> success_count{0};

    for (int i = 0; i < num_clients; ++i) {
        auto client = std::make_unique<TestClient>("stun_client" + std::to_string(i));
        client->ConnectToSignaling("127.0.0.1", env_->GetSignalingPort());
        clients.push_back(std::move(client));
    }

    std::vector<std::thread> threads;
    for (auto& client : clients) {
        threads.emplace_back([&client, &success_count, this]() {
            if (client->PerformStunBinding("127.0.0.1", env_->GetStunPort())) {
                success_count++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GE(success_count.load(), num_clients * 0.9);  // 90% success rate
}

TEST_F(MultiClientTest, ConcurrentTurnAllocations) {
    const int num_clients = 15;
    std::vector<std::unique_ptr<TestClient>> clients;
    std::atomic<int> success_count{0};

    for (int i = 0; i < num_clients; ++i) {
        auto client = std::make_unique<TestClient>("turn_client" + std::to_string(i));
        client->ConnectToSignaling("127.0.0.1", env_->GetSignalingPort());
        clients.push_back(std::move(client));
    }

    std::vector<std::thread> threads;
    for (auto& client : clients) {
        threads.emplace_back([&client, &success_count, this]() {
            if (client->AllocateRelay("127.0.0.1", env_->GetRelayPort())) {
                success_count++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GE(success_count.load(), num_clients * 0.8);  // 80% success rate
}

TEST_F(MultiClientTest, MeshNetwork) {
    const int num_clients = 5;
    std::vector<std::unique_ptr<TestClient>> clients;

    // Create and connect clients
    for (int i = 0; i < num_clients; ++i) {
        auto client = std::make_unique<TestClient>("mesh_client" + std::to_string(i));
        EXPECT_TRUE(client->ConnectToSignaling("127.0.0.1", env_->GetSignalingPort()));
        clients.push_back(std::move(client));
    }

    // Each client connects to all others
    std::atomic<int> connection_count{0};
    std::vector<std::thread> threads;

    for (size_t i = 0; i < clients.size(); ++i) {
        for (size_t j = i + 1; j < clients.size(); ++j) {
            threads.emplace_back([&clients, i, j, &connection_count]() {
                if (clients[i]->InitiateConnection(clients[j]->GetClientId())) {
                    connection_count++;
                }
            });
        }
    }

    for (auto& t : threads) {
        t.join();
    }

    // Expected connections: n*(n-1)/2 = 5*4/2 = 10
    EXPECT_GE(connection_count.load(), 8);  // At least 80% success
}

TEST_F(MultiClientTest, LoadTest) {
    const int num_operations = 100;
    std::atomic<int> success_count{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < num_operations; ++i) {
        threads.emplace_back([i, &success_count, this]() {
            TestClient client("load_client" + std::to_string(i));
            if (client.ConnectToSignaling("127.0.0.1", env_->GetSignalingPort())) {
                client.PerformStunBinding("127.0.0.1", env_->GetStunPort());
                success_count++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GE(success_count.load(), num_operations * 0.9);
}
