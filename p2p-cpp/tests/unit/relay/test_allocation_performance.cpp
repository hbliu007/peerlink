/**
 * @file test_allocation_performance.cpp
 * @brief Performance tests for allocation manager optimizations
 */

#include <gtest/gtest.h>
#include "p2p/servers/relay/allocation_manager.hpp"
#include <chrono>
#include <thread>
#include <vector>
#include <random>

using namespace p2p::relay;

class AllocationPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_unique<AllocationManager>(
            10000, 20000, 600, 5000);
        manager_->Start();
    }

    void TearDown() override {
        manager_->Stop();
    }

    std::unique_ptr<AllocationManager> manager_;
};

/**
 * Test: Measure lookup performance with many allocations
 */
TEST_F(AllocationPerformanceTest, LookupPerformanceWithManyAllocations) {
    // Create 1000 allocations
    std::vector<std::shared_ptr<TurnAllocation>> allocations;
    for (int i = 0; i < 1000; ++i) {
        Address client_addr("192.168.1." + std::to_string(i % 256),
                           10000 + i);
        auto alloc = manager_->CreateAllocation(
            client_addr, "10.0.0.1", TransportProtocol::UDP, 600);
        if (alloc) {
            allocations.push_back(alloc);
        }
    }

    ASSERT_GE(allocations.size(), 900);  // At least 90% success

    // Measure lookup by client address
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; ++i) {
        Address client_addr("192.168.1." + std::to_string(i % 256),
                           10000 + (i % 1000));
        auto alloc = manager_->GetAllocationByClient(client_addr);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Should complete 10k lookups in under 100ms
    EXPECT_LT(duration.count(), 100000);

    std::cout << "10k lookups took: " << duration.count() << " microseconds\n";
    std::cout << "Average per lookup: " << (duration.count() / 10000.0) << " microseconds\n";
}

/**
 * Test: Concurrent lookup performance
 */
TEST_F(AllocationPerformanceTest, ConcurrentLookupPerformance) {
    // Create 500 allocations
    std::vector<Address> client_addrs;
    for (int i = 0; i < 500; ++i) {
        Address client_addr("192.168.1." + std::to_string(i % 256),
                           10000 + i);
        auto alloc = manager_->CreateAllocation(
            client_addr, "10.0.0.1", TransportProtocol::UDP, 600);
        if (alloc) {
            client_addrs.push_back(client_addr);
        }
    }

    ASSERT_GE(client_addrs.size(), 450);

    // Launch 10 threads doing concurrent lookups
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    std::atomic<int> total_lookups{0};

    for (int t = 0; t < 10; ++t) {
        threads.emplace_back([this, &client_addrs, &total_lookups]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, client_addrs.size() - 1);

            for (int i = 0; i < 1000; ++i) {
                int idx = dis(gen);
                auto alloc = manager_->GetAllocationByClient(client_addrs[idx]);
                total_lookups++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(total_lookups, 10000);
    // 10 threads * 1000 lookups should complete in under 500ms
    EXPECT_LT(duration.count(), 500);

    std::cout << "10k concurrent lookups took: " << duration.count() << " ms\n";
}

/**
 * Test: Cleanup performance with many expired allocations
 */
TEST_F(AllocationPerformanceTest, CleanupPerformanceWithManyExpired) {
    // Create 1000 allocations with short lifetime
    for (int i = 0; i < 1000; ++i) {
        Address client_addr("192.168.1." + std::to_string(i % 256),
                           10000 + i);
        manager_->CreateAllocation(
            client_addr, "10.0.0.1", TransportProtocol::UDP, 1);  // 1 second lifetime
    }

    // Wait for allocations to expire
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Measure cleanup time
    auto start = std::chrono::high_resolution_clock::now();
    size_t cleaned = manager_->CleanupExpired();
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_GT(cleaned, 900);  // At least 90% cleaned
    // Cleanup should complete in under 100ms
    EXPECT_LT(duration.count(), 100);

    std::cout << "Cleaned " << cleaned << " allocations in " << duration.count() << " ms\n";
}

/**
 * Test: Mixed operations performance
 */
TEST_F(AllocationPerformanceTest, MixedOperationsPerformance) {
    std::atomic<int> creates{0};
    std::atomic<int> lookups{0};
    std::atomic<int> deletes{0};

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;

    // Thread 1: Create allocations
    threads.emplace_back([this, &creates]() {
        for (int i = 0; i < 200; ++i) {
            Address client_addr("10.0.1." + std::to_string(i % 256),
                               20000 + i);
            if (manager_->CreateAllocation(
                    client_addr, "10.0.0.1", TransportProtocol::UDP, 600)) {
                creates++;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    // Thread 2-5: Lookup allocations
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([this, &lookups, t]() {
            for (int i = 0; i < 500; ++i) {
                Address client_addr("10.0.1." + std::to_string((i + t * 50) % 256),
                                   20000 + (i + t * 50) % 200);
                manager_->GetAllocationByClient(client_addr);
                lookups++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Mixed operations: " << creates << " creates, "
              << lookups << " lookups in " << duration.count() << " ms\n";

    // Should complete in reasonable time
    EXPECT_LT(duration.count(), 1000);
}

/**
 * Test: Address ToString() optimization impact
 */
TEST_F(AllocationPerformanceTest, AddressToStringOptimization) {
    // Create test addresses
    std::vector<Address> addresses;
    for (int i = 0; i < 1000; ++i) {
        addresses.emplace_back("192.168." + std::to_string(i / 256) + "." +
                              std::to_string(i % 256), 10000 + i);
    }

    // Measure ToString() calls
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; ++i) {
        std::string str = addresses[i % 1000].ToString();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "10k ToString() calls took: " << duration.count() << " microseconds\n";

    // Now measure with caching (call once, reuse)
    std::vector<std::string> cached_strings;
    cached_strings.reserve(1000);
    for (const auto& addr : addresses) {
        cached_strings.push_back(addr.ToString());
    }

    start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; ++i) {
        const std::string& str = cached_strings[i % 1000];
    }

    end = std::chrono::high_resolution_clock::now();
    auto cached_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "10k cached string access took: " << cached_duration.count() << " microseconds\n";
    std::cout << "Speedup: " << (duration.count() / (double)cached_duration.count()) << "x\n";

    // Cached access should be significantly faster
    EXPECT_LT(cached_duration.count(), duration.count() / 5);
}

/**
 * Test: Verify optimization doesn't break correctness
 */
TEST_F(AllocationPerformanceTest, CorrectnessAfterOptimization) {
    // Create allocation
    Address client_addr("192.168.1.100", 12345);
    auto alloc = manager_->CreateAllocation(
        client_addr, "10.0.0.1", TransportProtocol::UDP, 600);

    ASSERT_NE(alloc, nullptr);

    // Verify lookup by client
    auto found_by_client = manager_->GetAllocationByClient(client_addr);
    ASSERT_NE(found_by_client, nullptr);
    EXPECT_EQ(found_by_client->GetAllocationId(), alloc->GetAllocationId());

    // Verify lookup by relay
    auto found_by_relay = manager_->GetAllocationByRelay(alloc->GetRelayAddr());
    ASSERT_NE(found_by_relay, nullptr);
    EXPECT_EQ(found_by_relay->GetAllocationId(), alloc->GetAllocationId());

    // Verify lookup by ID
    auto found_by_id = manager_->GetAllocation(alloc->GetAllocationId());
    ASSERT_NE(found_by_id, nullptr);
    EXPECT_EQ(found_by_id->GetAllocationId(), alloc->GetAllocationId());

    // Delete and verify
    EXPECT_TRUE(manager_->DeleteAllocation(alloc->GetAllocationId()));
    EXPECT_EQ(manager_->GetAllocation(alloc->GetAllocationId()), nullptr);
    EXPECT_EQ(manager_->GetAllocationByClient(client_addr), nullptr);
}

