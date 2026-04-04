# NAT Tests
find_package(GTest REQUIRED)

add_executable(test_stun_client test_stun_client.cpp)

target_compile_definitions(test_stun_client PRIVATE TESTING)

target_link_libraries(test_stun_client
    PRIVATE
    p2p_nat
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

target_include_directories(test_stun_client
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${Boost_INCLUDE_DIRS}
)

add_test(NAME STUNClientTest COMMAND test_stun_client)
