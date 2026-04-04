# Basic Client Example

add_executable(basic_client basic_client.cpp)

target_link_libraries(basic_client
    p2p_core
    p2p_protocol
    p2p_transport
    p2p_nat
    ${Boost_LIBRARIES}
    nlohmann_json::nlohmann_json
)

target_include_directories(basic_client PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
