# Core module tests

find_package(Boost 1.70 REQUIRED)
find_package(Threads REQUIRED)

# P2PClient tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_p2p_client.cpp)
    add_executable(test_p2p_client test_p2p_client.cpp)
    target_link_libraries(test_p2p_client
        PRIVATE
            p2p_core
            p2p_transport
            p2p_protocol
            Threads::Threads
            GTest::gtest
            GTest::gtest_main
    )
    add_test(NAME P2PClientTest COMMAND test_p2p_client)
endif()
