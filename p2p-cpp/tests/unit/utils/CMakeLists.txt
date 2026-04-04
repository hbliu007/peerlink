# Utils module tests

find_package(spdlog REQUIRED)

# Logger tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_logger.cpp)
    add_executable(test_logger test_logger.cpp)
    target_link_libraries(test_logger
        PRIVATE
            p2p_utils
            spdlog::spdlog
            GTest::gtest
            GTest::gtest_main
    )
    add_test(NAME LoggerTest COMMAND test_logger)
endif()

# ServiceConfig tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_service_config.cpp)
    add_executable(test_service_config test_service_config.cpp)
    target_link_libraries(test_service_config
        PRIVATE
            p2p_utils
            GTest::gtest
            GTest::gtest_main
    )
    add_test(NAME ServiceConfigTest COMMAND test_service_config)
endif()
