cmake_minimum_required(VERSION 3.20)

# Find GTest
find_package(GTest REQUIRED)

# STUN Message Test
add_executable(stun_message_test
    stun_message_test.cpp
)

target_link_libraries(stun_message_test
    PRIVATE
        p2p_protocol
        GTest::gtest
        GTest::gtest_main
)

# Add test
add_test(NAME StunMessageTest COMMAND stun_message_test)

# STUN Server Integration Test
add_executable(stun_server_test
    stun_server_test.cpp
)

target_link_libraries(stun_server_test
    PRIVATE
        stun_server
        GTest::gtest
        GTest::gtest_main
)

add_test(NAME StunServerTest COMMAND stun_server_test)
