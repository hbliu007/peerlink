# DID Service (STATIC for container deployment)
add_library(did_service STATIC
    did_server.cpp
    did_handler.cpp
    did_storage.cpp
    did_crypto.cpp
    did_auth.cpp
)

target_include_directories(did_service
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/servers/did
    PRIVATE
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(did_service
    PUBLIC
    Boost::asio
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
    p2p_utils
    relay_server
    PRIVATE
    Threads::Threads
)

# DID Server executable
add_executable(did-server
    main.cpp
)

target_link_libraries(did-server
    PRIVATE
    did_service
    p2p_utils
    p2p_security
)
