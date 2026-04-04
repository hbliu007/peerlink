add_library(p2p_protocol STATIC
    message.cpp
    stun.cpp
    dcutr.cpp
    negotiator.cpp
    dcutr_message.cpp
    relay_message.cpp
)

target_include_directories(p2p_protocol
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(p2p_protocol
    PUBLIC
        nlohmann_json::nlohmann_json
        p2p-proto
        protobuf::libprotobuf
)

set_target_properties(p2p_protocol PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
