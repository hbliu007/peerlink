# Security Tests

find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

# Test: Relay v2 Security (if exists)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_relay_v2_security.cpp)
    add_executable(test_relay_v2_security
        test_relay_v2_security.cpp
    )

    target_link_libraries(test_relay_v2_security
        PRIVATE
        relay_server
        p2p-crypto
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    target_include_directories(test_relay_v2_security
        PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )

    add_test(NAME RelayV2SecurityTest COMMAND test_relay_v2_security)
endif()
