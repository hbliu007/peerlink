# PeerLink Gateway (Signaling + DID unified binary)

add_executable(peerlink-gateway
    main.cpp
)

target_include_directories(peerlink-gateway
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/servers/signaling/include
        ${CMAKE_SOURCE_DIR}/src/servers/signaling/src
)

if(NOT APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
endif()

target_link_libraries(peerlink-gateway
    PRIVATE
        p2p_signaling_server
        did_service
        p2p_utils
        Boost::asio
        OpenSSL::SSL
        OpenSSL::Crypto
        nlohmann_json::nlohmann_json
        Threads::Threads
)

if(NOT APPLE)
    target_link_options(peerlink-gateway PRIVATE -Wl,--as-needed)
endif()
