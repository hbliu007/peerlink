# Third-party Dependencies

# Boost (required - header-only Asio)
find_package(Boost 1.74 REQUIRED)
if(Boost_FOUND)
    message(STATUS "Found Boost: ${Boost_VERSION}")
    include_directories(${Boost_INCLUDE_DIRS})
    add_definitions(-DBOOST_ASIO_HEADER_ONLY)
endif()

# OpenSSL (optional, for TLS/DTLS)
find_package(OpenSSL)
if(OPENSSL_FOUND)
    message(STATUS "Found OpenSSL: ${OPENSSL_VERSION}")
endif()

# GoogleTest (for testing)
if(BUILD_TESTS)
    find_package(GTest REQUIRED)
endif()

# nlohmann/json (header-only, use system include)
find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp
    PATHS /opt/homebrew/include /usr/local/include /usr/include
)
if(NLOHMANN_JSON_INCLUDE_DIR)
    message(STATUS "Found nlohmann_json: ${NLOHMANN_JSON_INCLUDE_DIR}")
    include_directories(${NLOHMANN_JSON_INCLUDE_DIR})

    # Create interface library for compatibility
    add_library(nlohmann_json INTERFACE)
    target_include_directories(nlohmann_json INTERFACE ${NLOHMANN_JSON_INCLUDE_DIR})
    add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
else()
    message(FATAL_ERROR "nlohmann_json not found")
endif()
