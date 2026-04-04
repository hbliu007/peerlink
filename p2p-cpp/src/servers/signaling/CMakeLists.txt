cmake_minimum_required(VERSION 3.20)

# Signaling Server Library (STATIC for container deployment)
add_library(p2p_signaling_server STATIC
    src/models.cpp
    src/connection_manager.cpp
    src/websocket_session.cpp
    src/register_handler.cpp
    src/connect_handler.cpp
    src/service_handler.cpp
    src/message_router.cpp
)

target_include_directories(p2p_signaling_server
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(p2p_signaling_server
    PUBLIC
        p2p_utils
        p2p_security
        did_service
        nlohmann_json::nlohmann_json
        OpenSSL::SSL
        OpenSSL::Crypto
        pthread
)

# Signaling Server Executable
add_executable(p2p-signaling-server
    src/main.cpp
)

target_link_libraries(p2p-signaling-server
    PRIVATE
        p2p_signaling_server
        p2p_utils
)

# Installation
install(TARGETS p2p_signaling_server p2p-signaling-server
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)