#include <gtest/gtest.h>
#include "servers/did/did_server.hpp"

using namespace p2p::did;

TEST(DIDServerTest, Construction) {
    DIDServerConfig config;
    config.host = "127.0.0.1";
    config.port = 8081;

    DIDServer server(config);
    EXPECT_TRUE(true);
}
