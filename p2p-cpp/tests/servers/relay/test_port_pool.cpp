// Port Pool Unit Tests

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include "p2p/servers/relay/port_pool.hpp"

using namespace p2p::relay;

class PortPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code
    }

    void TearDown() override {
        // Cleanup code
    }
};

TEST_F(PortPoolTest, AcquireAndRelease) {
    PortPool pool(10000, 10009);  // 10 ports

    // Acquire a port
    auto port = pool.Acquire();
    ASSERT_TRUE(port.has_value());
    EXPECT_GE(*port, 10000);
    EXPECT_LE(*port, 10009);

    // Pool should have one less port
    EXPECT_EQ(pool.AvailableCount(), 9);

    // Release the port
    EXPECT_TRUE(pool.Release(*port));
    EXPECT_EQ(pool.AvailableCount(), 10);
}

TEST_F(PortPoolTest, Exhaustion) {
    PortPool pool(10000, 10002);  // Only 3 ports

    // Acquire all ports
    std::vector<uint16_t> ports;
    for (int i = 0; i < 3; ++i) {
        auto port = pool.Acquire();
        ASSERT_TRUE(port.has_value());
        ports.push_back(*port);
    }

    // Next acquire should fail
    auto port = pool.Acquire();
    EXPECT_FALSE(port.has_value());

    // Release one port
    EXPECT_TRUE(pool.Release(ports[0]));

    // Should be able to acquire again
    port = pool.Acquire();
    EXPECT_TRUE(port.has_value());
}

TEST_F(PortPoolTest, UsagePercentage) {
    PortPool pool(10000, 10009);  // 10 ports

    EXPECT_DOUBLE_EQ(pool.UsagePercentage(), 0.0);

    pool.Acquire();
    EXPECT_DOUBLE_EQ(pool.UsagePercentage(), 10.0);

    pool.Acquire();
    EXPECT_DOUBLE_EQ(pool.UsagePercentage(), 20.0);
}

TEST_F(PortPoolTest, InvalidRelease) {
    PortPool pool(10000, 10009);

    // Try to release port outside range
    EXPECT_FALSE(pool.Release(9999));
    EXPECT_FALSE(pool.Release(10010));
}

TEST_F(PortPoolTest, ThreadSafety) {
    PortPool pool(10000, 10099);  // 100 ports

    std::vector<std::thread> threads;
    std::vector<std::optional<uint16_t>> acquired_ports(10);

    // Acquire ports from multiple threads
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&pool, &acquired_ports, i]() {
            acquired_ports[i] = pool.Acquire();
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // All acquisitions should succeed
    for (const auto& port : acquired_ports) {
        EXPECT_TRUE(port.has_value());
    }

    // All ports should be unique
    std::set<uint16_t> unique_ports;
    for (const auto& port : acquired_ports) {
        if (port) {
            unique_ports.insert(*port);
        }
    }
    EXPECT_EQ(unique_ports.size(), 10);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
