# Tests

# Add subdirectories
add_subdirectory(unit)
add_subdirectory(stun)
add_subdirectory(did)
add_subdirectory(detection)
add_subdirectory(nat)

# Integration tests (placeholder)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/integration/CMakeLists.txt)
    add_subdirectory(integration)
endif()

# Security tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/security/CMakeLists.txt)
    add_subdirectory(security)
endif()

# Benchmark tests (placeholder)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/CMakeLists.txt)
    add_subdirectory(benchmark)
endif()

# Server tests (placeholder)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/servers/CMakeLists.txt)
    add_subdirectory(servers)
endif()
