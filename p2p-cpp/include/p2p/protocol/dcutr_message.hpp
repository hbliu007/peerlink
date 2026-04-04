#pragma once

#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <chrono>

// Forward declare protobuf types
namespace p2p {
namespace protocol {
namespace dcutr {
class DCUtRMessage;
class Connect;
class Sync;
}
}
}

namespace p2p {
namespace protocol {

/**
 * DCUtR Message Wrapper
 *
 * Provides a C++ friendly interface to DCUtR protobuf messages
 */
class DCUtRMessageWrapper {
public:
    enum class Type {
        CONNECT,
        SYNC
    };

    DCUtRMessageWrapper();
    ~DCUtRMessageWrapper();

    // Disable copy, enable move
    DCUtRMessageWrapper(const DCUtRMessageWrapper&) = delete;
    DCUtRMessageWrapper& operator=(const DCUtRMessageWrapper&) = delete;
    DCUtRMessageWrapper(DCUtRMessageWrapper&&) noexcept;
    DCUtRMessageWrapper& operator=(DCUtRMessageWrapper&&) noexcept;

    /**
     * Create CONNECT message
     * @param addrs List of multiaddrs (as byte arrays)
     * @param timestamp_ns Timestamp in nanoseconds
     */
    static DCUtRMessageWrapper CreateConnect(
        const std::vector<std::vector<uint8_t>>& addrs,
        int64_t timestamp_ns);

    /**
     * Create SYNC message
     * @param addrs List of multiaddrs
     * @param echo_timestamp_ns Echo of initiator's timestamp
     * @param timestamp_ns Responder's timestamp
     */
    static DCUtRMessageWrapper CreateSync(
        const std::vector<std::vector<uint8_t>>& addrs,
        int64_t echo_timestamp_ns,
        int64_t timestamp_ns);

    /**
     * Serialize to bytes
     * @return Serialized message
     */
    std::vector<uint8_t> Serialize() const;

    /**
     * Deserialize from bytes
     * @param data Serialized message
     * @return Wrapper or nullopt on error
     */
    static std::optional<DCUtRMessageWrapper> Deserialize(
        const std::vector<uint8_t>& data);

    // Accessors
    Type GetType() const;

    // CONNECT accessors
    std::vector<std::vector<uint8_t>> GetConnectAddrs() const;
    int64_t GetConnectTimestamp() const;

    // SYNC accessors
    std::vector<std::vector<uint8_t>> GetSyncAddrs() const;
    int64_t GetSyncEchoTimestamp() const;
    int64_t GetSyncTimestamp() const;

private:
    explicit DCUtRMessageWrapper(std::unique_ptr<dcutr::DCUtRMessage> msg);

    std::unique_ptr<dcutr::DCUtRMessage> message_;
};

}  // namespace protocol
}  // namespace p2p
