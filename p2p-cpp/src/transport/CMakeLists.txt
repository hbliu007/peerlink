add_library(p2p_transport
    udp_transport.cpp
    relay_transport.cpp
)

target_include_directories(p2p_transport
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(p2p_transport
    PUBLIC
        Boost::asio
)

set_target_properties(p2p_transport PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
