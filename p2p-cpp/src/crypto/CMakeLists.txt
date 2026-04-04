# Crypto Library
cmake_minimum_required(VERSION 3.20)

# Source files
set(CRYPTO_SOURCES
    ed25519_signer.cpp
    signed_envelope.cpp
)

# Create crypto library
add_library(p2p-crypto STATIC ${CRYPTO_SOURCES})

# Link dependencies
target_link_libraries(p2p-crypto
    PUBLIC
        OpenSSL::Crypto
)

# Include directories
target_include_directories(p2p-crypto
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${Boost_INCLUDE_DIRS}
)

# Installation
install(TARGETS p2p-crypto
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
