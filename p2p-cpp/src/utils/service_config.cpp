#include "p2p/utils/service_config.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace p2p::utils {
namespace {

using ConfigValues = std::unordered_map<std::string, std::string>;

std::string Trim(std::string value) {
    auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(),
        [&](unsigned char ch) { return !is_space(ch); }));
    value.erase(std::find_if(value.rbegin(), value.rend(),
        [&](unsigned char ch) { return !is_space(ch); }).base(), value.end());
    return value;
}

std::string StripQuotes(std::string value) {
    value = Trim(std::move(value));
    if (value.size() >= 2 &&
        ((value.front() == '"' && value.back() == '"') ||
         (value.front() == '\'' && value.back() == '\''))) {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

std::string StripComments(std::string line) {
    bool in_single_quotes = false;
    bool in_double_quotes = false;
    for (std::size_t index = 0; index < line.size(); ++index) {
        const char ch = line[index];
        if (ch == '\'' && !in_double_quotes) {
            in_single_quotes = !in_single_quotes;
            continue;
        }
        if (ch == '"' && !in_single_quotes) {
            in_double_quotes = !in_double_quotes;
            continue;
        }
        if (ch == '#' && !in_single_quotes && !in_double_quotes) {
            return line.substr(0, index);
        }
    }
    return line;
}

ConfigValues ParseConfigFile(const std::string& path) {
    ConfigValues values;
    if (path.empty()) {
        return values;
    }

    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("Unable to open config file: " + path);
    }

    std::string section;
    std::string line;
    while (std::getline(input, line)) {
        line = StripComments(std::move(line));
        line = Trim(std::move(line));
        if (line.empty()) {
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            section = Trim(line.substr(1, line.size() - 2));
            continue;
        }

        std::size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) {
            continue;
        }

        std::string key = Trim(line.substr(0, eq_pos));
        std::string value = StripQuotes(line.substr(eq_pos + 1));
        std::string full_key = section.empty() ? key : section + "." + key;
        values[full_key] = value;
    }

    return values;
}

std::string GetEnv(const char* name) {
    const char* value = std::getenv(name);
    return (value != nullptr && value[0] != '\0') ? std::string(value) : std::string();
}

std::string GetString(const ConfigValues& values,
                      const std::string& key,
                      const std::string& default_value) {
    auto it = values.find(key);
    return it != values.end() ? it->second : default_value;
}

int GetInt(const ConfigValues& values, const std::string& key, int default_value) {
    auto it = values.find(key);
    if (it == values.end() || it->second.empty()) {
        return default_value;
    }
    return std::stoi(it->second);
}

bool GetBool(const ConfigValues& values, const std::string& key, bool default_value) {
    auto it = values.find(key);
    if (it == values.end()) {
        return default_value;
    }

    std::string value = it->second;
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (value == "true" || value == "1" || value == "yes" || value == "on") {
        return true;
    }
    if (value == "false" || value == "0" || value == "no" || value == "off") {
        return false;
    }
    return default_value;
}

void OverrideString(std::string& target, const char* env_name) {
    std::string value = GetEnv(env_name);
    if (!value.empty()) {
        target = value;
    }
}

void OverrideInt(int& target, const char* env_name) {
    std::string value = GetEnv(env_name);
    if (!value.empty()) {
        target = std::stoi(value);
    }
}

template <typename UInt>
void OverrideUInt(UInt& target, const char* env_name) {
    std::string value = GetEnv(env_name);
    if (!value.empty()) {
        target = static_cast<UInt>(std::stoul(value));
    }
}

void OverrideBool(bool& target, const char* env_name) {
    std::string value = GetEnv(env_name);
    if (value.empty()) {
        return;
    }
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    target = (value == "true" || value == "1" || value == "yes" || value == "on");
}

}  // namespace

std::string ResolveConfigPath(const std::string& explicit_path) {
    if (!explicit_path.empty()) {
        return explicit_path;
    }
    return GetEnv("PEERLINK_CONFIG");
}

NetworkServiceConfig LoadNetworkServiceConfig(const std::string& explicit_path) {
    NetworkServiceConfig config;
    config.config_path = ResolveConfigPath(explicit_path);
    config.admin.port = 9301;

    ConfigValues values = ParseConfigFile(config.config_path);
    config.stun_host = GetString(values, "stun.host", config.stun_host);
    config.stun_udp_port = static_cast<uint16_t>(GetInt(values, "stun.udp_port", config.stun_udp_port));
    config.stun_tcp_port = static_cast<uint16_t>(GetInt(values, "stun.tcp_port", config.stun_tcp_port));
    config.turn_host = GetString(values, "turn.host", config.turn_host);
    config.turn_port = static_cast<uint16_t>(GetInt(values, "turn.port", config.turn_port));
    config.turn_public_ip = GetString(values, "turn.public_ip", config.turn_public_ip);
    config.turn_min_port = static_cast<uint16_t>(GetInt(values, "turn.min_port", config.turn_min_port));
    config.turn_max_port = static_cast<uint16_t>(GetInt(values, "turn.max_port", config.turn_max_port));
    config.turn_lifetime = static_cast<uint32_t>(GetInt(values, "turn.lifetime", static_cast<int>(config.turn_lifetime)));
    config.turn_max_allocations = static_cast<std::size_t>(GetInt(values, "turn.max_allocations", static_cast<int>(config.turn_max_allocations)));
    config.metrics_interval_seconds = GetInt(values, "network.metrics_interval_seconds", config.metrics_interval_seconds);
    config.admin.enabled = GetBool(values, "network.admin.enabled",
                                   GetBool(values, "admin.enabled", config.admin.enabled));
    config.admin.host = GetString(values, "network.admin.host",
                                  GetString(values, "admin.host", config.admin.host));
    config.admin.port = static_cast<uint16_t>(GetInt(
        values, "network.admin.port",
        GetInt(values, "admin.port", config.admin.port)));

    OverrideString(config.stun_host, "STUN_HOST");
    OverrideUInt(config.stun_udp_port, "STUN_UDP_PORT");
    OverrideUInt(config.stun_tcp_port, "STUN_TCP_PORT");
    OverrideString(config.turn_host, "TURN_HOST");
    OverrideUInt(config.turn_port, "TURN_PORT");
    OverrideString(config.turn_public_ip, "TURN_PUBLIC_IP");
    OverrideUInt(config.turn_min_port, "TURN_MIN_PORT");
    OverrideUInt(config.turn_max_port, "TURN_MAX_PORT");
    OverrideUInt(config.turn_lifetime, "TURN_LIFETIME");
    OverrideUInt(config.turn_max_allocations, "TURN_MAX_ALLOCS");
    OverrideInt(config.metrics_interval_seconds, "NETWORK_METRICS_INTERVAL_SECONDS");
    OverrideBool(config.admin.enabled, "NETWORK_ADMIN_ENABLED");
    OverrideString(config.admin.host, "NETWORK_ADMIN_HOST");
    OverrideUInt(config.admin.port, "NETWORK_ADMIN_PORT");
    return config;
}

GatewayServiceConfig LoadGatewayServiceConfig(const std::string& explicit_path) {
    GatewayServiceConfig config;
    config.config_path = ResolveConfigPath(explicit_path);
    config.admin.port = 9300;

    ConfigValues values = ParseConfigFile(config.config_path);
    config.signaling_host = GetString(values, "signaling.host", config.signaling_host);
    config.signaling_port = static_cast<uint16_t>(GetInt(values, "signaling.port", config.signaling_port));
    config.heartbeat_interval = GetInt(values, "gateway.heartbeat_interval", config.heartbeat_interval);
    config.connection_timeout = GetInt(values, "gateway.connection_timeout", config.connection_timeout);
    config.allow_insecure_registration = GetBool(values, "gateway.allow_insecure_registration",
                                                 config.allow_insecure_registration);
    config.did_host = GetString(values, "did.host", config.did_host);
    config.did_port = static_cast<uint16_t>(GetInt(values, "did.port", config.did_port));
    config.turn_public_ip = GetString(values, "turn.public_ip", config.turn_public_ip);
    config.turn_port = static_cast<uint16_t>(GetInt(values, "turn.port", config.turn_port));
    config.jwt_secret = GetString(values, "auth.jwt_secret", config.jwt_secret);
    config.redis_host = GetString(values, "redis.host", config.redis_host);
    config.redis_port = static_cast<uint16_t>(GetInt(values, "redis.port", config.redis_port));
    config.admin.enabled = GetBool(values, "gateway.admin.enabled",
                                   GetBool(values, "admin.enabled", config.admin.enabled));
    config.admin.host = GetString(values, "gateway.admin.host",
                                  GetString(values, "admin.host", config.admin.host));
    config.admin.port = static_cast<uint16_t>(GetInt(
        values, "gateway.admin.port",
        GetInt(values, "admin.port", config.admin.port)));

    OverrideString(config.signaling_host, "SIGNALING_HOST");
    OverrideUInt(config.signaling_port, "SIGNALING_PORT");
    OverrideInt(config.heartbeat_interval, "HEARTBEAT_INTERVAL");
    OverrideInt(config.connection_timeout, "CONNECTION_TIMEOUT");
    OverrideBool(config.allow_insecure_registration, "SIGNALING_ALLOW_INSECURE_REGISTRATION");
    OverrideString(config.did_host, "DID_HOST");
    OverrideUInt(config.did_port, "DID_PORT");
    OverrideString(config.turn_public_ip, "TURN_PUBLIC_IP");
    OverrideUInt(config.turn_port, "TURN_PORT");
    OverrideString(config.jwt_secret, "JWT_SECRET");
    OverrideString(config.redis_host, "REDIS_HOST");
    OverrideUInt(config.redis_port, "REDIS_PORT");
    OverrideBool(config.admin.enabled, "GATEWAY_ADMIN_ENABLED");
    OverrideString(config.admin.host, "GATEWAY_ADMIN_HOST");
    OverrideUInt(config.admin.port, "GATEWAY_ADMIN_PORT");
    return config;
}

SignalingServiceConfig LoadSignalingServiceConfig(const std::string& explicit_path) {
    SignalingServiceConfig config;
    config.config_path = ResolveConfigPath(explicit_path);
    config.admin.port = 9302;

    ConfigValues values = ParseConfigFile(config.config_path);
    config.host = GetString(values, "signaling.host", config.host);
    config.port = static_cast<uint16_t>(GetInt(values, "signaling.port", config.port));
    config.heartbeat_interval = GetInt(values, "signaling.heartbeat_interval", config.heartbeat_interval);
    config.connection_timeout = GetInt(values, "signaling.connection_timeout", config.connection_timeout);
    config.allow_insecure_registration = GetBool(values, "signaling.allow_insecure_registration",
                                                 config.allow_insecure_registration);
    config.turn_public_ip = GetString(values, "turn.public_ip", config.turn_public_ip);
    config.turn_port = static_cast<uint16_t>(GetInt(values, "turn.port", config.turn_port));
    config.jwt_secret = GetString(values, "auth.jwt_secret", config.jwt_secret);
    config.admin.enabled = GetBool(values, "signaling.admin.enabled",
                                   GetBool(values, "admin.enabled", config.admin.enabled));
    config.admin.host = GetString(values, "signaling.admin.host",
                                  GetString(values, "admin.host", config.admin.host));
    config.admin.port = static_cast<uint16_t>(GetInt(
        values, "signaling.admin.port",
        GetInt(values, "admin.port", config.admin.port)));

    OverrideString(config.host, "SIGNALING_HOST");
    OverrideUInt(config.port, "SIGNALING_PORT");
    OverrideInt(config.heartbeat_interval, "HEARTBEAT_INTERVAL");
    OverrideInt(config.connection_timeout, "CONNECTION_TIMEOUT");
    OverrideBool(config.allow_insecure_registration, "SIGNALING_ALLOW_INSECURE_REGISTRATION");
    OverrideString(config.turn_public_ip, "TURN_PUBLIC_IP");
    OverrideUInt(config.turn_port, "TURN_PORT");
    OverrideString(config.jwt_secret, "JWT_SECRET");
    OverrideBool(config.admin.enabled, "SIGNALING_ADMIN_ENABLED");
    OverrideString(config.admin.host, "SIGNALING_ADMIN_HOST");
    OverrideUInt(config.admin.port, "SIGNALING_ADMIN_PORT");

    // Rate limiting overrides
    config.rate_limit.requests_per_second = static_cast<uint32_t>(GetInt(values, "signaling.rate_limit.rps",
        static_cast<int>(config.rate_limit.requests_per_second)));
    config.rate_limit.burst_size = static_cast<uint32_t>(GetInt(values, "signaling.rate_limit.burst",
        static_cast<int>(config.rate_limit.burst_size)));
    config.rate_limit.ban_threshold = static_cast<uint32_t>(GetInt(values, "signaling.rate_limit.ban_threshold",
        static_cast<int>(config.rate_limit.ban_threshold)));
    config.rate_limit.ban_duration_seconds = static_cast<uint32_t>(GetInt(values, "signaling.rate_limit.ban_duration",
        static_cast<int>(config.rate_limit.ban_duration_seconds)));
    OverrideUInt(config.rate_limit.requests_per_second, "SIGNALING_RATE_LIMIT_RPS");
    OverrideUInt(config.rate_limit.burst_size, "SIGNALING_RATE_LIMIT_BURST");
    OverrideUInt(config.rate_limit.ban_threshold, "SIGNALING_RATE_LIMIT_BAN_THRESHOLD");
    OverrideUInt(config.rate_limit.ban_duration_seconds, "SIGNALING_RATE_LIMIT_BAN_DURATION");

    // TLS configuration
    config.tls.enabled = GetBool(values, "signaling.tls.enabled", false);
    config.tls.cert_path = GetString(values, "signaling.tls.cert_path", "");
    config.tls.key_path = GetString(values, "signaling.tls.key_path", "");
    config.tls.ca_path = GetString(values, "signaling.tls.ca_path", "");
    config.tls.cipher_suites = GetString(values, "signaling.tls.cipher_suites", config.tls.cipher_suites);
    config.tls.require_client_cert = GetBool(values, "signaling.tls.require_client_cert", false);
    config.tls.handshake_timeout_ms = GetInt(values, "signaling.tls.handshake_timeout_ms", 10000);

    // TLS environment variable overrides
    OverrideBool(config.tls.enabled, "SIGNALING_TLS_ENABLED");
    OverrideString(config.tls.cert_path, "SIGNALING_TLS_CERT_PATH");
    OverrideString(config.tls.key_path, "SIGNALING_TLS_KEY_PATH");
    OverrideString(config.tls.ca_path, "SIGNALING_TLS_CA_PATH");

    return config;
}

DidServiceConfig LoadDidServiceConfig(const std::string& explicit_path) {
    DidServiceConfig config;
    config.config_path = ResolveConfigPath(explicit_path);
    config.admin.port = 9303;

    ConfigValues values = ParseConfigFile(config.config_path);
    config.host = GetString(values, "did.host", config.host);
    config.port = static_cast<uint16_t>(GetInt(values, "did.port", config.port));
    config.redis_host = GetString(values, "redis.host", config.redis_host);
    config.redis_port = static_cast<uint16_t>(GetInt(values, "redis.port", config.redis_port));
    config.jwt_secret = GetString(values, "auth.jwt_secret", config.jwt_secret);
    config.max_connections = GetInt(values, "did.max_connections", config.max_connections);
    config.admin.enabled = GetBool(values, "did.admin.enabled",
                                   GetBool(values, "admin.enabled", config.admin.enabled));
    config.admin.host = GetString(values, "did.admin.host",
                                  GetString(values, "admin.host", config.admin.host));
    config.admin.port = static_cast<uint16_t>(GetInt(
        values, "did.admin.port",
        GetInt(values, "admin.port", config.admin.port)));

    OverrideString(config.host, "DID_HOST");
    OverrideUInt(config.port, "DID_PORT");
    OverrideString(config.redis_host, "REDIS_HOST");
    OverrideUInt(config.redis_port, "REDIS_PORT");
    OverrideString(config.jwt_secret, "JWT_SECRET");
    OverrideInt(config.max_connections, "DID_MAX_CONNECTIONS");
    OverrideBool(config.admin.enabled, "DID_ADMIN_ENABLED");
    OverrideString(config.admin.host, "DID_ADMIN_HOST");
    OverrideUInt(config.admin.port, "DID_ADMIN_PORT");

    // Rate limiting overrides
    config.rate_limit.requests_per_second = static_cast<uint32_t>(GetInt(values, "did.rate_limit.rps",
        static_cast<int>(config.rate_limit.requests_per_second)));
    config.rate_limit.burst_size = static_cast<uint32_t>(GetInt(values, "did.rate_limit.burst",
        static_cast<int>(config.rate_limit.burst_size)));
    config.rate_limit.ban_threshold = static_cast<uint32_t>(GetInt(values, "did.rate_limit.ban_threshold",
        static_cast<int>(config.rate_limit.ban_threshold)));
    config.rate_limit.ban_duration_seconds = static_cast<uint32_t>(GetInt(values, "did.rate_limit.ban_duration",
        static_cast<int>(config.rate_limit.ban_duration_seconds)));
    OverrideUInt(config.rate_limit.requests_per_second, "DID_RATE_LIMIT_RPS");
    OverrideUInt(config.rate_limit.burst_size, "DID_RATE_LIMIT_BURST");
    OverrideUInt(config.rate_limit.ban_threshold, "DID_RATE_LIMIT_BAN_THRESHOLD");
    OverrideUInt(config.rate_limit.ban_duration_seconds, "DID_RATE_LIMIT_BAN_DURATION");

    // TLS configuration
    config.tls.enabled = GetBool(values, "did.tls.enabled", false);
    config.tls.cert_path = GetString(values, "did.tls.cert_path", "");
    config.tls.key_path = GetString(values, "did.tls.key_path", "");
    config.tls.ca_path = GetString(values, "did.tls.ca_path", "");
    config.tls.cipher_suites = GetString(values, "did.tls.cipher_suites", config.tls.cipher_suites);
    config.tls.require_client_cert = GetBool(values, "did.tls.require_client_cert", false);
    config.tls.handshake_timeout_ms = GetInt(values, "did.tls.handshake_timeout_ms", 10000);

    // TLS environment variable overrides
    OverrideBool(config.tls.enabled, "DID_TLS_ENABLED");
    OverrideString(config.tls.cert_path, "DID_TLS_CERT_PATH");
    OverrideString(config.tls.key_path, "DID_TLS_KEY_PATH");
    OverrideString(config.tls.ca_path, "DID_TLS_CA_PATH");

    return config;
}

}  // namespace p2p::utils
