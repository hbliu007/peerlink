# Transport module tests

find_package(Boost 1.70 REQUIRED)
find_package(Threads REQUIRED)

# MessageFramer tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_message_framer.cpp)
    add_executable(test_message_framer test_message_framer.cpp)
    target_link_libraries(test_message_framer
        PRIVATE
            p2p_transport
            GTest::gtest
            GTest::gtest_main
    )
    add_test(NAME MessageFramerTest COMMAND test_message_framer)
endif()

# RelayTransport tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_relay_transport.cpp)
    add_executable(test_relay_transport test_relay_transport.cpp)
    target_link_libraries(test_relay_transport
        PRIVATE
            p2p_transport
            Threads::Threads
            GTest::gtest
            GTest::gtest_main
    )
    add_test(NAME RelayTransportTest COMMAND test_relay_transport)
endif()

# UDPTransport tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_udp_transport.cpp)
    add_executable(test_udp_transport test_udp_transport.cpp)
    target_link_libraries(test_udp_transport
        PRIVATE
            p2p_transport
            Threads::Threads
            GTest::gtest
            GTest::gtest_main
    )
    add_test(NAME UDPTransportTest COMMAND test_udp_transport)
endif()
