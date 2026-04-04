find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

add_executable(test_signaling_message_handler
    test_message_handler.cpp
)

target_link_libraries(test_signaling_message_handler
    PRIVATE
        p2p_signaling_server
        relay_server
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
)

add_test(NAME SignalingMessageHandlerTest COMMAND test_signaling_message_handler)

add_executable(test_signaling_connection_manager
    test_connection_manager.cpp
)

target_link_libraries(test_signaling_connection_manager
    PRIVATE
        p2p_signaling_server
        relay_server
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
)

add_test(NAME SignalingConnectionManagerTest COMMAND test_signaling_connection_manager)

