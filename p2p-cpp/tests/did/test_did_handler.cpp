#include "servers/did/did_handler.hpp"
#include <gtest/gtest.h>

using namespace p2p::did;

class DIDHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

TEST_F(DIDHandlerTest, HandleBasicRequest) {
    EXPECT_NO_THROW(HandleDIDRequest());
}

TEST_F(DIDHandlerTest, HandleMultipleRequests) {
    EXPECT_NO_THROW({
        HandleDIDRequest();
        HandleDIDRequest();
        HandleDIDRequest();
    });
}

TEST_F(DIDHandlerTest, ThreadSafety) {
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([]() {
            HandleDIDRequest();
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    SUCCEED();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
