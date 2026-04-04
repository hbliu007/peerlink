# Relay server unit tests

find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

# Test: Allocation Manager
add_executable(test_allocation test_allocation.cpp)
target_link_libraries(test_allocation
    PRIVATE
    relay_server
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)
add_test(NAME AllocationManagerTest COMMAND test_allocation)

# Test: Bandwidth Limiter
add_executable(test_bandwidth test_bandwidth.cpp)
target_link_libraries(test_bandwidth
    PRIVATE
    relay_server
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)
add_test(NAME BandwidthLimiterTest COMMAND test_bandwidth)

# Test: Port Pool
add_executable(test_port_pool test_port_pool.cpp)
target_link_libraries(test_port_pool
    PRIVATE
    relay_server
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)
add_test(NAME PortPoolTest COMMAND test_port_pool)

# Test: TURN Message
add_executable(test_turn_message test_turn_message.cpp)
target_link_libraries(test_turn_message
    PRIVATE
    relay_server
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)
add_test(NAME TurnMessageTest COMMAND test_turn_message)

# Test: Relay Server Integration
add_executable(test_relay_server test_relay_server.cpp)
target_link_libraries(test_relay_server
    PRIVATE
    relay_server
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)
add_test(NAME RelayServerTest COMMAND test_relay_server)

# Test: Reservation Manager
add_executable(test_reservation test_reservation.cpp)
target_link_libraries(test_reservation
    PRIVATE
    relay_server
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)
add_test(NAME ReservationManagerTest COMMAND test_reservation)

