# Net Tests
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

add_executable(test_socket
    test_socket.cpp
)

target_link_libraries(test_socket
    PRIVATE
    p2p_net
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

target_include_directories(test_socket
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

add_test(NAME SocketTest COMMAND test_socket)