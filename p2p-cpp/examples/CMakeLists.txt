# Examples

# DCUtR NAT Integration example
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/dcutr_nat_integration.cpp)
    add_executable(dcutr_nat_integration dcutr_nat_integration.cpp)
    target_link_libraries(dcutr_nat_integration
        PRIVATE
            p2p_protocol
            p2p_nat
    )
endif()

# Basic examples
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/basic/CMakeLists.txt)
    add_subdirectory(basic)
endif()

# Advanced examples (placeholder)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/advanced/CMakeLists.txt)
    add_subdirectory(advanced)
endif()
