# Find GTest
find_package(GTest REQUIRED)

add_executable(device_detector_test
    device_detector_test.cpp
)

target_link_libraries(device_detector_test
    PRIVATE
        p2p_detection
        GTest::gtest
        GTest::gtest_main
)

add_test(NAME DeviceDetectorTest COMMAND device_detector_test)

# Device vendor tests
add_executable(test_device_vendor
    test_device_vendor.cpp
)

target_link_libraries(test_device_vendor
    PRIVATE
        p2p_detection
        GTest::gtest
        GTest::gtest_main
)

add_test(NAME DeviceVendorTest COMMAND test_device_vendor)

