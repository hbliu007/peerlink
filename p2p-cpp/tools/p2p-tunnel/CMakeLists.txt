# P2P Tunnel Tool
cmake_minimum_required(VERSION 3.20)

# Tunnel Client
add_executable(p2p-tunnel-client tunnel_client.cpp)
target_link_libraries(p2p-tunnel-client
    p2p_core
    p2p_transport
    p2p_nat
    p2p_protocol
    Boost::asio
    Threads::Threads
)

# Tunnel Server
add_executable(p2p-tunnel-server tunnel_server.cpp)
target_link_libraries(p2p-tunnel-server
    p2p_core
    p2p_transport
    p2p_nat
    p2p_protocol
    Boost::asio
    Threads::Threads
)

# Simple Relay Server (lightweight TCP relay for firewall traversal)
add_executable(simple-relay-server simple_relay_server.cpp)
target_link_libraries(simple-relay-server
    Boost::asio
    Threads::Threads
)

# Install targets
install(TARGETS p2p-tunnel-client p2p-tunnel-server simple-relay-server
    RUNTIME DESTINATION bin
)
