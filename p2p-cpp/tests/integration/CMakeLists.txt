# Integration Tests

find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

# Test: End-to-End Integration (if exists)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_e2e_integration.cpp)
    add_executable(test_e2e_integration
        test_e2e_integration.cpp
        test_helpers.cpp
    )

    target_link_libraries(test_e2e_integration
        PRIVATE
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    target_include_directories(test_e2e_integration
        PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${Boost_INCLUDE_DIRS}
    )

    add_test(NAME E2EIntegrationTest COMMAND test_e2e_integration)
endif()

# Test: Multi-client concurrent (if exists)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_multi_client.cpp)
    add_executable(test_multi_client
        test_multi_client.cpp
        test_helpers.cpp
    )

    target_link_libraries(test_multi_client
        PRIVATE
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    target_include_directories(test_multi_client
        PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${Boost_INCLUDE_DIRS}
    )

    add_test(NAME MultiClientTest COMMAND test_multi_client)
endif()

# Test: DCUtR Integration (if exists)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_dcutr_integration.cpp)
    add_executable(test_dcutr_integration
        test_dcutr_integration.cpp
    )

    target_link_libraries(test_dcutr_integration
        PRIVATE
        p2p_protocol
        p2p_nat
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    target_include_directories(test_dcutr_integration
        PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )

    add_test(NAME DCUtRIntegrationTest COMMAND test_dcutr_integration)
endif()

# Test: Relay v2 Integration (if exists)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_relay_v2_integration.cpp)
    add_executable(test_relay_v2_integration
        test_relay_v2_integration.cpp
    )

    target_link_libraries(test_relay_v2_integration
        PRIVATE
        relay_server
        p2p_net
        p2p-crypto
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    target_include_directories(test_relay_v2_integration
        PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )

    add_test(NAME RelayV2IntegrationTest COMMAND test_relay_v2_integration)
endif()

# Test: P2P Connection Acceptance Tests (NEW)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_p2p_connection.cpp)
    add_executable(test_p2p_connection
        test_p2p_connection.cpp
        p2p_test_framework.cpp
    )

    target_link_libraries(test_p2p_connection
        PRIVATE
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    target_include_directories(test_p2p_connection
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${Boost_INCLUDE_DIRS}
    )

    add_test(NAME P2PConnectionTest COMMAND test_p2p_connection)

    # Set test properties
    set_tests_properties(P2PConnectionTest PROPERTIES
        TIMEOUT 60
        LABELS "integration;p2p;acceptance"
    )
endif()
