add_library(p2p_core
    p2p_client.cpp
    varint.cpp
    multiaddr.cpp
)

target_include_directories(p2p_core
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(p2p_core
    PUBLIC
        p2p_protocol
        p2p_transport
        p2p_nat
        Boost::asio
)

set_target_properties(p2p_core PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
