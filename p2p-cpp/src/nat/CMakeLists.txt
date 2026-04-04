add_library(p2p_nat
    stun_client.cpp
    puncher.cpp
    multiaddr_converter.cpp
    connection.cpp
)

target_include_directories(p2p_nat
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(p2p_nat
    PUBLIC
        Boost::asio
        p2p_protocol
        p2p_net
)

set_target_properties(p2p_nat PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
