#include "servers/did/did_storage.hpp"

namespace p2p {
namespace did {

DidStorage::DidStorage(const std::string& redis_host, uint16_t redis_port)
    : redis_host_(redis_host), redis_port_(redis_port) {
}

bool DidStorage::Store(const std::string& key, const std::string& value) {
    if (key.empty()) {
        return false;
    }

    std::unique_lock lock(storage_mutex_);
    storage_[key] = value;
    return true;
}

std::string DidStorage::Retrieve(const std::string& key) {
    if (key.empty()) {
        return {};
    }

    std::shared_lock lock(storage_mutex_);
    const auto it = storage_.find(key);
    if (it == storage_.end()) {
        return {};
    }
    return it->second;
}

bool DidStorage::DeleteKey(const std::string& key) {
    if (key.empty()) {
        return false;
    }

    std::unique_lock lock(storage_mutex_);
    return storage_.erase(key) > 0;
}

} // namespace did
} // namespace p2p
