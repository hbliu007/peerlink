# Relay Server CMakeLists.txt

cmake_minimum_required(VERSION 3.20)

# Source files
set(RELAY_SERVER_SOURCES
    turn_message.cpp
    port_pool.cpp
    allocation_manager.cpp
    bandwidth_limiter.cpp
    relay_server.cpp
    rate_limiter.cpp
)

# Create relay server library
add_library(relay_server STATIC ${RELAY_SERVER_SOURCES})

target_include_directories(relay_server
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${Boost_INCLUDE_DIRS}
)

target_link_libraries(relay_server
    PUBLIC
        pthread
        p2p-crypto
        p2p_net
        p2p_utils
)

# C++20 standard
target_compile_features(relay_server PUBLIC cxx_std_20)

# Compiler warnings
if(MSVC)
    target_compile_options(relay_server PRIVATE /W4)
else()
    target_compile_options(relay_server PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Install
install(TARGETS relay_server
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Create relay server executable
add_executable(relay_server_bin main.cpp)

target_link_libraries(relay_server_bin
    PRIVATE
        relay_server
)

set_target_properties(relay_server_bin
    PROPERTIES
        OUTPUT_NAME relay_server
)

# Install executable
install(TARGETS relay_server_bin
    RUNTIME DESTINATION bin
)
