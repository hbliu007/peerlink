#pragma once

#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace p2p {
namespace did {

class DidStorage {
public:
    DidStorage(const std::string& redis_host, uint16_t redis_port);

    bool Store(const std::string& key, const std::string& value);
    std::string Retrieve(const std::string& key);
    bool DeleteKey(const std::string& key);
    bool Delete(const std::string& key) { return DeleteKey(key); }

private:
    std::string redis_host_;
    uint16_t redis_port_;
    std::unordered_map<std::string, std::string> storage_;
    mutable std::shared_mutex storage_mutex_;
};

using DIDStorage = DidStorage;

} // namespace did
} // namespace p2p
