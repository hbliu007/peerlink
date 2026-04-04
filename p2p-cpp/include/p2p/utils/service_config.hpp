#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace p2p::utils {

struct TlsConfig {
    bool enabled = false;
    std::string cert_path;
    std::string key_path;
    std::string ca_path;
    std::string cipher_suites = "TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256:TLS_AES_128_GCM_SHA256";
    bool require_client_cert = false;
    int handshake_timeout_ms = 10000;
};

struct AdminConfig {
    bool enabled = true;
    std::string host = "127.0.0.1";
    uint16_t port = 0;
};

struct RateLimitServiceConfig {
    uint32_t requests_per_second = 10;
    uint32_t burst_size = 20;
    uint32_t ban_threshold = 5;
    uint32_t ban_duration_seconds = 300;
};

struct NetworkServiceConfig {
    std::string config_path;
    std::string stun_host = "0.0.0.0";
    uint16_t stun_udp_port = 3478;
    uint16_t stun_tcp_port = 3479;
    std::string turn_host = "0.0.0.0";
    uint16_t turn_port = 9001;
    std::string turn_public_ip = "127.0.0.1";
    uint16_t turn_min_port = 50000;
    uint16_t turn_max_port = 50100;
    uint32_t turn_lifetime = 600;
    std::size_t turn_max_allocations = 1000;
    int metrics_interval_seconds = 30;
    AdminConfig admin;
};

struct GatewayServiceConfig {
    std::string config_path;
    std::string signaling_host = "0.0.0.0";
    uint16_t signaling_port = 8080;
    int heartbeat_interval = 30;
    int connection_timeout = 90;
    bool allow_insecure_registration = false;
    std::string did_host = "0.0.0.0";
    uint16_t did_port = 8081;
    std::string turn_public_ip = "127.0.0.1";
    uint16_t turn_port = 9001;
    std::string jwt_secret;
    std::string redis_host = "127.0.0.1";
    uint16_t redis_port = 6379;
    AdminConfig admin;
};

struct SignalingServiceConfig {
    std::string config_path;
    std::string host = "0.0.0.0";
    uint16_t port = 8080;
    int heartbeat_interval = 30;
    int connection_timeout = 90;
    bool allow_insecure_registration = false;
    std::string turn_public_ip = "127.0.0.1";
    uint16_t turn_port = 9001;
    std::string jwt_secret;
    AdminConfig admin;
    RateLimitServiceConfig rate_limit = {20, 50, 5, 300};  // 20 msg/s, burst 50
    TlsConfig tls;
};

struct DidServiceConfig {
    std::string config_path;
    std::string host = "0.0.0.0";
    uint16_t port = 8081;
    std::string redis_host = "127.0.0.1";
    uint16_t redis_port = 6379;
    std::string jwt_secret;
    int max_connections = 1000;
    AdminConfig admin;
    RateLimitServiceConfig rate_limit = {10, 20, 5, 300};  // 10 req/s, burst 20
    TlsConfig tls;
};

std::string ResolveConfigPath(const std::string& explicit_path = {});
NetworkServiceConfig LoadNetworkServiceConfig(const std::string& explicit_path = {});
GatewayServiceConfig LoadGatewayServiceConfig(const std::string& explicit_path = {});
SignalingServiceConfig LoadSignalingServiceConfig(const std::string& explicit_path = {});
DidServiceConfig LoadDidServiceConfig(const std::string& explicit_path = {});

}  // namespace p2p::utils
