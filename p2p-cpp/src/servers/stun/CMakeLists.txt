cmake_minimum_required(VERSION 3.20)

# STUN Server Library (STATIC for container deployment)
add_library(stun_server STATIC
    stun_server.cpp
)

target_link_libraries(stun_server
    PUBLIC
        p2p_protocol
        p2p_utils
        pthread
)

target_include_directories(stun_server
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# STUN Server Executable
add_executable(stun_server_bin
    main.cpp
)

target_link_libraries(stun_server_bin
    PRIVATE
        stun_server
)

set_target_properties(stun_server_bin PROPERTIES
    OUTPUT_NAME stun_server
)

# Installation
install(TARGETS stun_server_bin
    RUNTIME DESTINATION bin
)

# Benchmark Tool
add_executable(stun_benchmark
    benchmark.cpp
)

target_link_libraries(stun_benchmark
    PRIVATE
        p2p_protocol
)

install(TARGETS stun_benchmark
    RUNTIME DESTINATION bin
)
