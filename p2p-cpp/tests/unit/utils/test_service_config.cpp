#include "p2p/utils/service_config.hpp"
#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <cstdlib>

using namespace p2p::utils;
namespace fs = std::filesystem;

class ServiceConfigTest : public ::testing::Test {
protected:
    std::string test_config_file_;
    std::vector<std::string> env_vars_to_clear_;

    void SetUp() override {
        // Generate unique config file name
        test_config_file_ = "test_config_" +
                           std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) +
                           ".ini";
    }

    void TearDown() override {
        // Clean up config file
        if (fs::exists(test_config_file_)) {
            fs::remove(test_config_file_);
        }

        // Clear environment variables
        for (const auto& var : env_vars_to_clear_) {
            unsetenv(var.c_str());
        }
        env_vars_to_clear_.clear();
    }

    // Helper: Write config file
    void WriteConfigFile(const std::string& content) {
        std::ofstream file(test_config_file_);
        file << content;
        file.close();
    }

    // Helper: Set environment variable and track for cleanup
    void SetEnv(const std::string& name, const std::string& value) {
        setenv(name.c_str(), value.c_str(), 1);
        env_vars_to_clear_.push_back(name);
    }
};

// --- ResolveConfigPath Tests ---

TEST_F(ServiceConfigTest, ResolveConfigPath_WithExplicitPath_ReturnsExplicitPath) {
    std::string explicit_path = "/path/to/config.ini";
    auto result = ResolveConfigPath(explicit_path);
    EXPECT_EQ(result, explicit_path);
}

TEST_F(ServiceConfigTest, ResolveConfigPath_WithoutExplicitPath_ReturnsEnvVar) {
    SetEnv("PEERLINK_CONFIG", "/env/config.ini");
    auto result = ResolveConfigPath();
    EXPECT_EQ(result, "/env/config.ini");
}

TEST_F(ServiceConfigTest, ResolveConfigPath_NoPathNoEnv_ReturnsEmpty) {
    auto result = ResolveConfigPath();
    EXPECT_TRUE(result.empty());
}

// --- NetworkServiceConfig Tests ---

TEST_F(ServiceConfigTest, LoadNetworkServiceConfig_EmptyFile_UsesDefaults) {
    WriteConfigFile("");

    auto config = LoadNetworkServiceConfig(test_config_file_);

    EXPECT_EQ(config.stun_host, "0.0.0.0");
    EXPECT_EQ(config.stun_udp_port, 3478);
    EXPECT_EQ(config.stun_tcp_port, 3479);
    EXPECT_EQ(config.turn_host, "0.0.0.0");
    EXPECT_EQ(config.turn_port, 9001);
    EXPECT_EQ(config.turn_public_ip, "127.0.0.1");
    EXPECT_EQ(config.turn_min_port, 50000);
    EXPECT_EQ(config.turn_max_port, 50100);
    EXPECT_EQ(config.turn_lifetime, 600);
    EXPECT_EQ(config.turn_max_allocations, 1000);
    EXPECT_EQ(config.metrics_interval_seconds, 30);
    EXPECT_TRUE(config.admin.enabled);
    EXPECT_EQ(config.admin.host, "127.0.0.1");
    EXPECT_EQ(config.admin.port, 9301);
}

TEST_F(ServiceConfigTest, LoadNetworkServiceConfig_WithValues_LoadsCorrectly) {
    WriteConfigFile(R"(
[stun]
host = 192.168.1.100
udp_port = 3500
tcp_port = 3501

[turn]
host = 192.168.1.101
port = 9002
public_ip = 203.0.113.1
min_port = 60000
max_port = 60100
lifetime = 1200
max_allocations = 2000

[network]
metrics_interval_seconds = 60

[admin]
enabled = false
host = 0.0.0.0
port = 9400
)");

    auto config = LoadNetworkServiceConfig(test_config_file_);

    EXPECT_EQ(config.stun_host, "192.168.1.100");
    EXPECT_EQ(config.stun_udp_port, 3500);
    EXPECT_EQ(config.stun_tcp_port, 3501);
    EXPECT_EQ(config.turn_host, "192.168.1.101");
    EXPECT_EQ(config.turn_port, 9002);
    EXPECT_EQ(config.turn_public_ip, "203.0.113.1");
    EXPECT_EQ(config.turn_min_port, 60000);
    EXPECT_EQ(config.turn_max_port, 60100);
    EXPECT_EQ(config.turn_lifetime, 1200);
    EXPECT_EQ(config.turn_max_allocations, 2000);
    EXPECT_EQ(config.metrics_interval_seconds, 60);
    EXPECT_FALSE(config.admin.enabled);
    EXPECT_EQ(config.admin.host, "0.0.0.0");
    EXPECT_EQ(config.admin.port, 9400);
}

TEST_F(ServiceConfigTest, LoadNetworkServiceConfig_EnvOverride_OverridesFileValues) {
    WriteConfigFile(R"(
[stun]
host = 192.168.1.100
udp_port = 3500
)");

    SetEnv("STUN_HOST", "10.0.0.1");
    SetEnv("STUN_UDP_PORT", "4000");

    auto config = LoadNetworkServiceConfig(test_config_file_);

    EXPECT_EQ(config.stun_host, "10.0.0.1");
    EXPECT_EQ(config.stun_udp_port, 4000);
}

TEST_F(ServiceConfigTest, LoadNetworkServiceConfig_WithComments_IgnoresComments) {
    WriteConfigFile(R"(
# This is a comment
[stun]
host = 192.168.1.100  # inline comment
# Another comment
udp_port = 3500
)");

    auto config = LoadNetworkServiceConfig(test_config_file_);

    EXPECT_EQ(config.stun_host, "192.168.1.100");
    EXPECT_EQ(config.stun_udp_port, 3500);
}

TEST_F(ServiceConfigTest, LoadNetworkServiceConfig_WithQuotes_StripQuotes) {
    WriteConfigFile(R"(
[stun]
host = "192.168.1.100"
)");

    auto config = LoadNetworkServiceConfig(test_config_file_);

    EXPECT_EQ(config.stun_host, "192.168.1.100");
}

TEST_F(ServiceConfigTest, LoadNetworkServiceConfig_WithSingleQuotes_StripQuotes) {
    WriteConfigFile(R"(
[stun]
host = '192.168.1.100'
)");

    auto config = LoadNetworkServiceConfig(test_config_file_);

    EXPECT_EQ(config.stun_host, "192.168.1.100");
}

TEST_F(ServiceConfigTest, LoadNetworkServiceConfig_WithWhitespace_TrimsWhitespace) {
    WriteConfigFile(R"(
[stun]
host =   192.168.1.100
udp_port  =  3500
)");

    auto config = LoadNetworkServiceConfig(test_config_file_);

    EXPECT_EQ(config.stun_host, "192.168.1.100");
    EXPECT_EQ(config.stun_udp_port, 3500);
}

// --- GatewayServiceConfig Tests ---

TEST_F(ServiceConfigTest, LoadGatewayServiceConfig_EmptyFile_UsesDefaults) {
    WriteConfigFile("");

    auto config = LoadGatewayServiceConfig(test_config_file_);

    EXPECT_EQ(config.signaling_host, "0.0.0.0");
    EXPECT_EQ(config.signaling_port, 8080);
    EXPECT_EQ(config.heartbeat_interval, 30);
    EXPECT_EQ(config.connection_timeout, 90);
    EXPECT_FALSE(config.allow_insecure_registration);
    EXPECT_EQ(config.did_host, "0.0.0.0");
    EXPECT_EQ(config.did_port, 8081);
    EXPECT_EQ(config.turn_public_ip, "127.0.0.1");
    EXPECT_EQ(config.turn_port, 9001);
    EXPECT_TRUE(config.jwt_secret.empty());
    EXPECT_EQ(config.redis_host, "127.0.0.1");
    EXPECT_EQ(config.redis_port, 6379);
    EXPECT_TRUE(config.admin.enabled);
    EXPECT_EQ(config.admin.port, 9300);
}

TEST_F(ServiceConfigTest, LoadGatewayServiceConfig_WithValues_LoadsCorrectly) {
    WriteConfigFile(R"(
[signaling]
host = 192.168.1.200
port = 8888

[gateway]
heartbeat_interval = 60
connection_timeout = 120
allow_insecure_registration = true

[did]
host = 192.168.1.201
port = 8889

[turn]
public_ip = 203.0.113.2
port = 9003

[auth]
jwt_secret = my_secret_key

[redis]
host = 192.168.1.202
port = 6380
)");

    auto config = LoadGatewayServiceConfig(test_config_file_);

    EXPECT_EQ(config.signaling_host, "192.168.1.200");
    EXPECT_EQ(config.signaling_port, 8888);
    EXPECT_EQ(config.heartbeat_interval, 60);
    EXPECT_EQ(config.connection_timeout, 120);
    EXPECT_TRUE(config.allow_insecure_registration);
    EXPECT_EQ(config.did_host, "192.168.1.201");
    EXPECT_EQ(config.did_port, 8889);
    EXPECT_EQ(config.turn_public_ip, "203.0.113.2");
    EXPECT_EQ(config.turn_port, 9003);
    EXPECT_EQ(config.jwt_secret, "my_secret_key");
    EXPECT_EQ(config.redis_host, "192.168.1.202");
    EXPECT_EQ(config.redis_port, 6380);
}

// --- SignalingServiceConfig Tests ---

TEST_F(ServiceConfigTest, LoadSignalingServiceConfig_EmptyFile_UsesDefaults) {
    WriteConfigFile("");

    auto config = LoadSignalingServiceConfig(test_config_file_);

    EXPECT_EQ(config.host, "0.0.0.0");
    EXPECT_EQ(config.port, 8080);
    EXPECT_EQ(config.heartbeat_interval, 30);
    EXPECT_EQ(config.connection_timeout, 90);
    EXPECT_FALSE(config.allow_insecure_registration);
    EXPECT_EQ(config.turn_public_ip, "127.0.0.1");
    EXPECT_EQ(config.turn_port, 9001);
    EXPECT_TRUE(config.jwt_secret.empty());
    EXPECT_TRUE(config.admin.enabled);
    EXPECT_EQ(config.admin.port, 9302);
    EXPECT_EQ(config.rate_limit.requests_per_second, 20);
    EXPECT_EQ(config.rate_limit.burst_size, 50);
    EXPECT_EQ(config.rate_limit.ban_threshold, 5);
    EXPECT_EQ(config.rate_limit.ban_duration_seconds, 300);
    EXPECT_FALSE(config.tls.enabled);
}

TEST_F(ServiceConfigTest, LoadSignalingServiceConfig_WithRateLimit_LoadsCorrectly) {
    WriteConfigFile(R"(
[signaling]
host = 192.168.1.300
port = 9999

[signaling.rate_limit]
rps = 100
burst = 200
ban_threshold = 10
ban_duration = 600
)");

    auto config = LoadSignalingServiceConfig(test_config_file_);

    EXPECT_EQ(config.host, "192.168.1.300");
    EXPECT_EQ(config.port, 9999);
    EXPECT_EQ(config.rate_limit.requests_per_second, 100);
    EXPECT_EQ(config.rate_limit.burst_size, 200);
    EXPECT_EQ(config.rate_limit.ban_threshold, 10);
    EXPECT_EQ(config.rate_limit.ban_duration_seconds, 600);
}

TEST_F(ServiceConfigTest, LoadSignalingServiceConfig_WithTLS_LoadsCorrectly) {
    WriteConfigFile(R"(
[signaling.tls]
enabled = true
cert_path = /path/to/cert.pem
key_path = /path/to/key.pem
ca_path = /path/to/ca.pem
require_client_cert = true
handshake_timeout_ms = 5000
)");

    auto config = LoadSignalingServiceConfig(test_config_file_);

    EXPECT_TRUE(config.tls.enabled);
    EXPECT_EQ(config.tls.cert_path, "/path/to/cert.pem");
    EXPECT_EQ(config.tls.key_path, "/path/to/key.pem");
    EXPECT_EQ(config.tls.ca_path, "/path/to/ca.pem");
    EXPECT_TRUE(config.tls.require_client_cert);
    EXPECT_EQ(config.tls.handshake_timeout_ms, 5000);
}

// --- DidServiceConfig Tests ---

TEST_F(ServiceConfigTest, LoadDidServiceConfig_EmptyFile_UsesDefaults) {
    WriteConfigFile("");

    auto config = LoadDidServiceConfig(test_config_file_);

    EXPECT_EQ(config.host, "0.0.0.0");
    EXPECT_EQ(config.port, 8081);
    EXPECT_EQ(config.redis_host, "127.0.0.1");
    EXPECT_EQ(config.redis_port, 6379);
    EXPECT_TRUE(config.jwt_secret.empty());
    EXPECT_EQ(config.max_connections, 1000);
    EXPECT_TRUE(config.admin.enabled);
    EXPECT_EQ(config.admin.port, 9303);
    EXPECT_EQ(config.rate_limit.requests_per_second, 10);
    EXPECT_EQ(config.rate_limit.burst_size, 20);
    EXPECT_FALSE(config.tls.enabled);
}

TEST_F(ServiceConfigTest, LoadDidServiceConfig_WithValues_LoadsCorrectly) {
    WriteConfigFile(R"(
[did]
host = 192.168.1.400
port = 8888
max_connections = 5000

[redis]
host = 192.168.1.401
port = 6380

[auth]
jwt_secret = did_secret

[did.rate_limit]
rps = 50
burst = 100
)");

    auto config = LoadDidServiceConfig(test_config_file_);

    EXPECT_EQ(config.host, "192.168.1.400");
    EXPECT_EQ(config.port, 8888);
    EXPECT_EQ(config.max_connections, 5000);
    EXPECT_EQ(config.redis_host, "192.168.1.401");
    EXPECT_EQ(config.redis_port, 6380);
    EXPECT_EQ(config.jwt_secret, "did_secret");
    EXPECT_EQ(config.rate_limit.requests_per_second, 50);
    EXPECT_EQ(config.rate_limit.burst_size, 100);
}

// --- Boolean Parsing Tests ---

TEST_F(ServiceConfigTest, BooleanParsing_TrueVariants_ParsedAsTrue) {
    WriteConfigFile(R"(
[test]
bool1 = true
bool2 = TRUE
bool3 = 1
bool4 = yes
bool5 = YES
bool6 = on
bool7 = ON
)");

    // Use admin.enabled as test field
    WriteConfigFile(R"(
[admin]
enabled = true
)");
    auto config1 = LoadNetworkServiceConfig(test_config_file_);
    EXPECT_TRUE(config1.admin.enabled);

    WriteConfigFile(R"(
[admin]
enabled = 1
)");
    auto config2 = LoadNetworkServiceConfig(test_config_file_);
    EXPECT_TRUE(config2.admin.enabled);

    WriteConfigFile(R"(
[admin]
enabled = yes
)");
    auto config3 = LoadNetworkServiceConfig(test_config_file_);
    EXPECT_TRUE(config3.admin.enabled);

    WriteConfigFile(R"(
[admin]
enabled = on
)");
    auto config4 = LoadNetworkServiceConfig(test_config_file_);
    EXPECT_TRUE(config4.admin.enabled);
}

TEST_F(ServiceConfigTest, BooleanParsing_FalseVariants_ParsedAsFalse) {
    WriteConfigFile(R"(
[admin]
enabled = false
)");
    auto config1 = LoadNetworkServiceConfig(test_config_file_);
    EXPECT_FALSE(config1.admin.enabled);

    WriteConfigFile(R"(
[admin]
enabled = 0
)");
    auto config2 = LoadNetworkServiceConfig(test_config_file_);
    EXPECT_FALSE(config2.admin.enabled);

    WriteConfigFile(R"(
[admin]
enabled = no
)");
    auto config3 = LoadNetworkServiceConfig(test_config_file_);
    EXPECT_FALSE(config3.admin.enabled);

    WriteConfigFile(R"(
[admin]
enabled = off
)");
    auto config4 = LoadNetworkServiceConfig(test_config_file_);
    EXPECT_FALSE(config4.admin.enabled);
}

// --- Edge Cases ---

TEST_F(ServiceConfigTest, EmptyLines_Ignored) {
    WriteConfigFile(R"(

[stun]

host = 192.168.1.100


udp_port = 3500

)");

    auto config = LoadNetworkServiceConfig(test_config_file_);

    EXPECT_EQ(config.stun_host, "192.168.1.100");
    EXPECT_EQ(config.stun_udp_port, 3500);
}

TEST_F(ServiceConfigTest, MissingEquals_LineIgnored) {
    WriteConfigFile(R"(
[stun]
host = 192.168.1.100
invalid_line_without_equals
udp_port = 3500
)");

    auto config = LoadNetworkServiceConfig(test_config_file_);

    EXPECT_EQ(config.stun_host, "192.168.1.100");
    EXPECT_EQ(config.stun_udp_port, 3500);
}

TEST_F(ServiceConfigTest, NonExistentFile_ThrowsException) {
    EXPECT_THROW(LoadNetworkServiceConfig("/nonexistent/config.ini"), std::runtime_error);
}

TEST_F(ServiceConfigTest, CommentInQuotes_NotStripped) {
    WriteConfigFile(R"(
[stun]
host = "192.168.1.100 # not a comment"
)");

    auto config = LoadNetworkServiceConfig(test_config_file_);

    EXPECT_EQ(config.stun_host, "192.168.1.100 # not a comment");
}

TEST_F(ServiceConfigTest, NestedSections_FlattenedCorrectly) {
    WriteConfigFile(R"(
[network.admin]
enabled = false
host = 10.0.0.1
port = 9999
)");

    auto config = LoadNetworkServiceConfig(test_config_file_);

    EXPECT_FALSE(config.admin.enabled);
    EXPECT_EQ(config.admin.host, "10.0.0.1");
    EXPECT_EQ(config.admin.port, 9999);
}
