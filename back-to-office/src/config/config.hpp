#pragma once

#include <map>
#include <string>
#include <optional>

namespace bto::config {
    struct HostConfig {
        std::string did;
        std::string user;
        std::string key;    // SSH private key path (optional)
        uint16_t port = 22;
    };

    struct Config {
        std::string relay;
        std::string identity;
        std::map<std::string, HostConfig> hosts;

        static auto load(const std::string& path) -> std::optional<Config>;
        auto save(const std::string& path) const -> bool;
        auto resolve_host(const std::string& input) const -> std::optional<HostConfig>;
    };
}
