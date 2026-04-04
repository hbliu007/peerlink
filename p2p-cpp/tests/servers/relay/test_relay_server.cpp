#include <gtest/gtest.h>
#include "p2p/servers/relay/allocation_manager.hpp"
#include "p2p/servers/relay/port_pool.hpp"
#include <thread>
#include <chrono>

using namespace p2p::relay;

// Test PortPool
class PortPoolTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(PortPoolTest, AcquirePort) {
    PortPool pool(50000, 50100);
    auto port = pool.Acquire();
    EXPECT_TRUE(port.has_value());
    EXPECT_GE(port.value(), 50000);
    EXPECT_LE(port.value(), 50100);
}

TEST_F(PortPoolTest, ReleasePort) {
    PortPool pool(50000, 50010);
    auto port = pool.Acquire();
    ASSERT_TRUE(port.has_value());

    pool.Release(port.value());

    // Should be able to allocate again
    auto port2 = pool.Acquire();
    EXPECT_TRUE(port2.has_value());
}

TEST_F(PortPoolTest, PortExhaustion) {
    PortPool pool(50000, 50004);  // Only 5 ports (50000-50004 inclusive)

    std::vector<uint16_t> allocated;
    for (int i = 0; i < 6; ++i) {
        auto port = pool.Acquire();
        if (port.has_value()) {
            allocated.push_back(port.value());
        }
    }

    // Should allocate at most 5 ports
    EXPECT_LE(allocated.size(), 5u);
}

TEST_F(PortPoolTest, GetAvailableCount) {
    PortPool pool(50000, 50010);
    size_t initial = pool.AvailableCount();

    auto port = pool.Acquire();
    ASSERT_TRUE(port.has_value());

    size_t after = pool.AvailableCount();
    EXPECT_EQ(after, initial - 1);
}

// Test AllocationManager (basic functionality)
class AllocationManagerBasicTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_unique<AllocationManager>(50000, 50100, 600, 100);
    }

    std::unique_ptr<AllocationManager> manager_;
};

TEST_F(AllocationManagerBasicTest, CreateAllocation) {
    Address client_addr("192.168.1.100", 12345);
    auto alloc = manager_->CreateAllocation(client_addr, "127.0.0.1", TransportProtocol::UDP, 600);

    ASSERT_NE(alloc, nullptr);
    EXPECT_EQ(alloc->GetClientAddr(), client_addr);
    EXPECT_GT(alloc->GetRelayAddr().port, 0);
}

TEST_F(AllocationManagerBasicTest, GetAllocationByClient) {
    Address client_addr("192.168.1.101", 12346);
    auto alloc1 = manager_->CreateAllocation(client_addr, "127.0.0.1", TransportProtocol::UDP, 600);
    ASSERT_NE(alloc1, nullptr);

    auto alloc2 = manager_->GetAllocationByClient(client_addr);
    ASSERT_NE(alloc2, nullptr);
    EXPECT_EQ(alloc2->GetClientAddr(), client_addr);
}

TEST_F(AllocationManagerBasicTest, RefreshAllocation) {
    Address client_addr("192.168.1.102", 12347);
    auto alloc = manager_->CreateAllocation(client_addr, "127.0.0.1", TransportProtocol::UDP, 600);
    ASSERT_NE(alloc, nullptr);

    bool refreshed = manager_->RefreshAllocation(alloc->GetAllocationId(), 1200);
    EXPECT_TRUE(refreshed);
}

TEST_F(AllocationManagerBasicTest, DeleteAllocation) {
    Address client_addr("192.168.1.103", 12348);
    auto alloc = manager_->CreateAllocation(client_addr, "127.0.0.1", TransportProtocol::UDP, 600);
    ASSERT_NE(alloc, nullptr);

    bool deleted = manager_->DeleteAllocation(alloc->GetAllocationId());
    EXPECT_TRUE(deleted);

    auto alloc2 = manager_->GetAllocationByClient(client_addr);
    EXPECT_EQ(alloc2, nullptr);
}

TEST_F(AllocationManagerBasicTest, MaxAllocations) {
    AllocationManager small_manager(50000, 50005, 600, 3);  // Max 3 allocations

    std::vector<Address> clients = {
        Address("192.168.1.1", 10001),
        Address("192.168.1.2", 10002),
        Address("192.168.1.3", 10003),
        Address("192.168.1.4", 10004)
    };

    int success_count = 0;
    for (const auto& client : clients) {
        auto alloc = small_manager.CreateAllocation(client, "127.0.0.1", TransportProtocol::UDP, 600);
        if (alloc != nullptr) {
            success_count++;
        }
    }

    EXPECT_LE(success_count, 3);
}

TEST_F(AllocationManagerBasicTest, GetStatistics) {
    Address client1("192.168.1.104", 12349);
    Address client2("192.168.1.105", 12350);

    manager_->CreateAllocation(client1, "127.0.0.1", TransportProtocol::UDP, 600);
    manager_->CreateAllocation(client2, "127.0.0.1", TransportProtocol::UDP, 600);

    auto stats = manager_->GetStats();
    EXPECT_EQ(stats.active_allocations, 2u);
    EXPECT_GE(stats.total_allocations, 2u);
}
