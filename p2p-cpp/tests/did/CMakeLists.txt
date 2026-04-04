# DID Service Tests
find_package(GTest REQUIRED)

# Check if all test files exist
set(DID_TEST_FILES
    test_did_crypto.cpp
    test_did_storage.cpp
    test_did_auth.cpp
    test_did_server.cpp
)

set(ALL_FILES_EXIST TRUE)
foreach(file ${DID_TEST_FILES})
    if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${file})
        set(ALL_FILES_EXIST FALSE)
        message(STATUS "DID test file missing: ${file}")
    endif()
endforeach()

if(ALL_FILES_EXIST)
    add_executable(test_did_service ${DID_TEST_FILES})

    target_link_libraries(test_did_service
        PRIVATE
        did_service
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    target_include_directories(test_did_service
        PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${Boost_INCLUDE_DIRS}
    )

    add_test(NAME DIDServiceTest COMMAND test_did_service)
else()
    message(STATUS "Skipping DID tests - some files are missing")
endif()
