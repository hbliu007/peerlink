cmake_minimum_required(VERSION 3.20)

# Detection library
add_library(p2p_detection
    device_vendor.cpp
    device_detector.cpp
)

target_include_directories(p2p_detection
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(p2p_detection
    PUBLIC
        Threads::Threads
)

# Set C++20 standard
target_compile_features(p2p_detection PUBLIC cxx_std_20)
