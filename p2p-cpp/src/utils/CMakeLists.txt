# Utils Module
add_library(p2p_utils STATIC
    logger.cpp
    service_config.cpp
    admin_http_server.cpp
)

target_include_directories(p2p_utils PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(p2p_utils PUBLIC
    spdlog::spdlog
    fmt::fmt
    Boost::asio
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)
