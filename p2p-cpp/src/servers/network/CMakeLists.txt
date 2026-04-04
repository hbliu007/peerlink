# Network Server CMakeLists.txt — unified STUN + TURN/Relay binary

cmake_minimum_required(VERSION 3.20)

add_executable(peerlink-network main.cpp)

if(NOT APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
endif()

target_link_libraries(peerlink-network
    PRIVATE
        stun_server
        relay_server
        p2p_utils
        nlohmann_json::nlohmann_json
)

if(NOT APPLE)
    target_link_options(peerlink-network PRIVATE -Wl,--as-needed)
endif()

target_compile_features(peerlink-network PUBLIC cxx_std_20)

install(TARGETS peerlink-network
    RUNTIME DESTINATION bin
)
