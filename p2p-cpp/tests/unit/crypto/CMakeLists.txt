# Crypto Unit Tests
cmake_minimum_required(VERSION 3.20)

# Test files
set(CRYPTO_TEST_SOURCES
    test_ed25519_signer.cpp
    test_signed_envelope.cpp
)

# Create test executable
add_executable(crypto_test ${CRYPTO_TEST_SOURCES})

# Link dependencies
target_link_libraries(crypto_test
    PRIVATE
        p2p-crypto
        GTest::gtest
        GTest::gtest_main
)

# Add to CTest
add_test(NAME crypto_test COMMAND crypto_test)

# Set test properties
set_tests_properties(crypto_test PROPERTIES
    TIMEOUT 30
    LABELS "unit;crypto"
)
