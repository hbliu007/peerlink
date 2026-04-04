# Net library
add_library(p2p_net
    socket.cpp
)

target_include_directories(p2p_net
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(p2p_net
    PUBLIC
    Threads::Threads
)