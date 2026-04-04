cmake_minimum_required(VERSION 3.20)
project(P2P_Platform VERSION 1.0.0 LANGUAGES CXX C)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build Options
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_EXAMPLES "Build examples" ON)
option(BUILD_SERVERS "Build server components" ON)
option(BUILD_BINDINGS_PYTHON "Build Python bindings" ON)
option(BUILD_BINDINGS_JAVA "Build Java bindings" OFF)
option(BUILD_BINDINGS_SWIFT "Build Swift bindings" OFF)
option(BUILD_BINDINGS_JS "Build JavaScript bindings" OFF)
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(ENABLE_COVERAGE "Enable code coverage" OFF)

# Platform Detection
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(P2P_PLATFORM_LINUX ON)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(P2P_PLATFORM_MACOS ON)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(P2P_PLATFORM_WINDOWS ON)
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(P2P_PLATFORM_IOS ON)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(P2P_PLATFORM_ANDROID ON)
endif()

# Compiler Flags
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    # Temporarily disable -Werror for rapid development
    # add_compile_options(-Werror)
endif()

# Sanitizers
if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

if(ENABLE_TSAN)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
endif()

# Code Coverage
if(ENABLE_COVERAGE)
    add_compile_options(--coverage)
    add_link_options(--coverage)
endif()

# Include Directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${Boost_INCLUDE_DIRS})

# CMake Modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Find Dependencies
find_package(Boost 1.70 REQUIRED)
find_package(OpenSSL REQUIRED)

# Threads: macOS with Miniforge clang needs explicit pthread handling
set(CMAKE_THREAD_LIBS_INIT "-lpthread")
set(CMAKE_HAVE_THREADS_LIBRARY 1)
set(CMAKE_USE_PTHREADS_INIT 1)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

find_package(Protobuf REQUIRED)

# fmt must be found before spdlog to ensure spdlog uses external fmt
# Force use of Homebrew fmt (not Miniforge)
set(CMAKE_PREFIX_PATH "/opt/homebrew;${CMAKE_PREFIX_PATH}")
find_package(fmt REQUIRED)
set(SPDLOG_FMT_EXTERNAL ON CACHE BOOL "Use external fmt library" FORCE)
find_package(spdlog REQUIRED)

# Third-party Dependencies
add_subdirectory(third_party)

# Protobuf Messages
add_subdirectory(proto)

# Create Boost::asio interface library (header-only)
if(NOT TARGET Boost::asio)
    add_library(Boost::asio INTERFACE IMPORTED)
    set_target_properties(Boost::asio PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS}"
    )
endif()

# Core Library
add_subdirectory(src/core)
add_subdirectory(src/protocol)
add_subdirectory(src/transport)
add_subdirectory(src/nat)
add_subdirectory(src/net)
add_subdirectory(src/security)
add_subdirectory(src/crypto)
add_subdirectory(src/utils)
add_subdirectory(src/platform)
add_subdirectory(src/detection)

# Language Bindings
if(BUILD_BINDINGS_PYTHON)
    if(EXISTS ${CMAKE_SOURCE_DIR}/src/bindings/python/CMakeLists.txt)
        add_subdirectory(src/bindings/python)
    else()
        message(STATUS "Python bindings directory not found, skipping")
    endif()
endif()

if(BUILD_BINDINGS_JAVA)
    add_subdirectory(src/bindings/java)
endif()

if(BUILD_BINDINGS_SWIFT)
    add_subdirectory(src/bindings/swift)
endif()

if(BUILD_BINDINGS_JS)
    add_subdirectory(src/bindings/javascript)
endif()

# Server Components
if(BUILD_SERVERS)
    add_subdirectory(src/servers/stun)
    add_subdirectory(src/servers/relay)
    add_subdirectory(src/servers/signaling)
    add_subdirectory(src/servers/did)
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Installation
install(DIRECTORY include/p2p
    DESTINATION include
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# Package Configuration
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/P2PPlatformConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/P2PPlatformConfigVersion.cmake"
    DESTINATION lib/cmake/P2PPlatform
)
