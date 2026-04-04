// Allocation Manager Unit Tests

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include "p2p/servers/relay/allocation_manager.hpp"

using namespace p2p::relay;

class AllocationManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager = std::make_unique<AllocationManager>(10000, 10009, 600, 100);
    }

    void TearDown() override {
        if (manager) {
            manager->Stop();
        }
    }

    std::unique_ptr<AllocationManager> manager;
};

TEST_F(AllocationManagerTest, CreateAllocation) {
    Address client_addr("192.168.1.1", 12345);

    auto allocation = manager->CreateAllocation(
        client_addr,
        "10.0.0.1",
        TransportProtocol::UDP,
        600);

    ASSERT_NE(allocation, nullptr);
    EXPECT_FALSE(allocation->GetAllocationId().empty());
    EXPECT_EQ(allocation->GetClientAddr().ip, "192.168.1.1");
    EXPECT_EQ(allocation->GetClientAddr().port, 12345);
    EXPECT_GE(allocation->GetRelayAddr().port, 10000);
    EXPECT_LE(allocation->GetRelayAddr().port, 10009);
    EXPECT_EQ(allocation->GetTransport(), TransportProtocol::UDP);
}

TEST_F(AllocationManagerTest, DuplicateClientAllocation) {
    Address client_addr("192.168.1.1", 12345);

    auto allocation1 = manager->CreateAllocation(client_addr, "10.0.0.1");
    ASSERT_NE(allocation1, nullptr);

    // Try to create another allocation for same client
    auto allocation2 = manager->CreateAllocation(client_addr, "10.0.0.1");
    EXPECT_EQ(allocation2, nullptr);
}

TEST_F(AllocationManagerTest, GetAllocationByClient) {
    Address client_addr("192.168.1.1", 12345);

    auto allocation = manager->CreateAllocation(client_addr, "10.0.0.1");
    ASSERT_NE(allocation, nullptr);

    auto found = manager->GetAllocationByClient(client_addr);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->GetAllocationId(), allocation->GetAllocationId());
}

TEST_F(AllocationManagerTest, GetAllocationByRelay) {
    Address client_addr("192.168.1.1", 12345);

    auto allocation = manager->CreateAllocation(client_addr, "10.0.0.1");
    ASSERT_NE(allocation, nullptr);

    auto found = manager->GetAllocationByRelay(allocation->GetRelayAddr());
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->GetAllocationId(), allocation->GetAllocationId());
}

TEST_F(AllocationManagerTest, RefreshAllocation) {
    Address client_addr("192.168.1.1", 12345);

    auto allocation = manager->CreateAllocation(client_addr, "10.0.0.1", TransportProtocol::UDP, 10);
    ASSERT_NE(allocation, nullptr);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    uint32_t remaining_before = allocation->GetRemainingTime();

    bool refreshed = manager->RefreshAllocation(allocation->GetAllocationId(), 100);
    EXPECT_TRUE(refreshed);

    uint32_t remaining_after = allocation->GetRemainingTime();
    EXPECT_GT(remaining_after, remaining_before);
}

TEST_F(AllocationManagerTest, DeleteAllocation) {
    Address client_addr("192.168.1.1", 12345);

    auto allocation = manager->CreateAllocation(client_addr, "10.0.0.1");
    ASSERT_NE(allocation, nullptr);

    std::string allocation_id = allocation->GetAllocationId();

    bool deleted = manager->DeleteAllocation(allocation_id);
    EXPECT_TRUE(deleted);

    auto found = manager->GetAllocation(allocation_id);
    EXPECT_EQ(found, nullptr);
}

TEST_F(AllocationManagerTest, AllocationExpiration) {
    Address client_addr("192.168.1.1", 12345);

    auto allocation = manager->CreateAllocation(
        client_addr, "10.0.0.1", TransportProtocol::UDP, 1);
    ASSERT_NE(allocation, nullptr);

    EXPECT_FALSE(allocation->IsExpired());

    // Sleep for 1.5 seconds to account for timer precision issues
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    EXPECT_TRUE(allocation->IsExpired());
}

TEST_F(AllocationManagerTest, CleanupExpired) {
    Address client1("192.168.1.1", 12345);
    Address client2("192.168.1.2", 12346);

    auto alloc1 = manager->CreateAllocation(client1, "10.0.0.1", TransportProtocol::UDP, 1);
    auto alloc2 = manager->CreateAllocation(client2, "10.0.0.1", TransportProtocol::UDP, 600);

    ASSERT_NE(alloc1, nullptr);
    ASSERT_NE(alloc2, nullptr);

    // Sleep for 1.5 seconds to account for timer precision issues
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    size_t cleaned = manager->CleanupExpired();
    EXPECT_EQ(cleaned, 1);

    EXPECT_EQ(manager->GetAllocation(alloc1->GetAllocationId()), nullptr);
    EXPECT_NE(manager->GetAllocation(alloc2->GetAllocationId()), nullptr);
}

TEST_F(AllocationManagerTest, MaxAllocations) {
    AllocationManager small_manager(10000, 10002, 600, 3);

    Address client1("192.168.1.1", 12345);
    Address client2("192.168.1.2", 12346);
    Address client3("192.168.1.3", 12347);
    Address client4("192.168.1.4", 12348);

    EXPECT_NE(small_manager.CreateAllocation(client1, "10.0.0.1"), nullptr);
    EXPECT_NE(small_manager.CreateAllocation(client2, "10.0.0.1"), nullptr);
    EXPECT_NE(small_manager.CreateAllocation(client3, "10.0.0.1"), nullptr);
    EXPECT_EQ(small_manager.CreateAllocation(client4, "10.0.0.1"), nullptr);
}

TEST_F(AllocationManagerTest, Statistics) {
    Address client1("192.168.1.1", 12345);
    Address client2("192.168.1.2", 12346);

    auto alloc1 = manager->CreateAllocation(client1, "10.0.0.1");
    auto alloc2 = manager->CreateAllocation(client2, "10.0.0.1");

    ASSERT_NE(alloc1, nullptr);
    ASSERT_NE(alloc2, nullptr);

    alloc1->RecordSent(1000);
    alloc1->RecordReceived(2000);
    alloc2->RecordSent(500);
    alloc2->RecordReceived(1500);

    auto stats = manager->GetStats();
    EXPECT_EQ(stats.total_allocations, 2);
    EXPECT_EQ(stats.active_allocations, 2);
    EXPECT_EQ(stats.total_bytes_sent, 1500);
    EXPECT_EQ(stats.total_bytes_received, 3500);
}

TEST_F(AllocationManagerTest, PermissionManagement) {
    Address client_addr("192.168.1.1", 12345);
    Address peer_addr("10.0.0.2", 60000);

    auto allocation = manager->CreateAllocation(client_addr, "10.0.0.1");
    ASSERT_NE(allocation, nullptr);

    EXPECT_FALSE(allocation->HasPermission(peer_addr));

    bool added = allocation->AddPermission(peer_addr);
    EXPECT_TRUE(added);
    EXPECT_TRUE(allocation->HasPermission(peer_addr));

    // Try to add duplicate
    added = allocation->AddPermission(peer_addr);
    EXPECT_FALSE(added);
}

TEST_F(AllocationManagerTest, ConcurrentAllocations) {
    std::vector<std::thread> threads;
    std::vector<std::shared_ptr<TurnAllocation>> allocations(10);

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, i, &allocations]() {
            Address client_addr("192.168.1." + std::to_string(i), 12345 + i);
            allocations[i] = manager->CreateAllocation(client_addr, "10.0.0.1");
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    int successful = 0;
    for (const auto& alloc : allocations) {
        if (alloc != nullptr) {
            successful++;
        }
    }

    EXPECT_EQ(successful, 10);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
