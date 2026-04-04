# Security Module
add_library(p2p_security STATIC
    tls_context.cpp
)

target_include_directories(p2p_security PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(p2p_security PUBLIC
    OpenSSL::SSL
    OpenSSL::Crypto
    Boost::asio
    spdlog::spdlog
    fmt::fmt
    p2p_utils
)
