#pragma once

#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <cstdint>

// Forward declare protobuf types
namespace p2p {
namespace relay {
namespace v2 {
class CircuitRelay;
class Reservation;
class Peer;
class Status;
}
}
}

namespace p2p {
namespace protocol {

/**
 * Relay v2 Status Codes
 */
enum class RelayStatusCode {
    OK = 0,
    RESERVATION_REFUSED = 1,
    RESOURCE_LIMIT_EXCEEDED = 2,
    PERMISSION_DENIED = 3,
    CONNECTION_FAILED = 4,
    NO_RESERVATION = 5,
    MALFORMED_MESSAGE = 6,
    UNEXPECTED_MESSAGE = 7
};

/**
 * Relay v2 Message Type
 */
enum class RelayMessageType {
    RESERVE,
    CONNECT,
    STATUS
};

/**
 * Reservation Information
 */
struct ReservationInfo {
    uint64_t expire;           // Unix timestamp (seconds)
    std::vector<uint8_t> addr; // Relay multiaddr
    std::vector<uint8_t> voucher;  // Signed envelope
    uint64_t limit_duration;   // Max duration (seconds)
    uint64_t limit_data;       // Max data (bytes)
};

/**
 * Peer Information
 */
struct PeerInfo {
    std::vector<uint8_t> id;   // Peer ID
    std::vector<std::vector<uint8_t>> addrs;  // Multiaddrs
};

/**
 * Relay v2 Message Wrapper
 *
 * Provides a C++ friendly interface to Relay v2 protobuf messages
 */
class RelayMessageWrapper {
public:
    RelayMessageWrapper();
    ~RelayMessageWrapper();

    // Disable copy, enable move
    RelayMessageWrapper(const RelayMessageWrapper&) = delete;
    RelayMessageWrapper& operator=(const RelayMessageWrapper&) = delete;
    RelayMessageWrapper(RelayMessageWrapper&&) noexcept;
    RelayMessageWrapper& operator=(RelayMessageWrapper&&) noexcept;

    /**
     * Create RESERVE message
     */
    static RelayMessageWrapper CreateReserve();

    /**
     * Create CONNECT message
     * @param peer Peer to connect to
     */
    static RelayMessageWrapper CreateConnect(const PeerInfo& peer);

    /**
     * Create STATUS message
     * @param code Status code
     * @param text Optional status text
     * @param reservation Optional reservation info
     */
    static RelayMessageWrapper CreateStatus(
        RelayStatusCode code,
        const std::string& text = "",
        const std::optional<ReservationInfo>& reservation = std::nullopt);

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
    static std::optional<RelayMessageWrapper> Deserialize(
        const std::vector<uint8_t>& data);

    // Accessors
    RelayMessageType GetType() const;

    // RESERVE accessors (no additional data)

    // CONNECT accessors
    std::optional<PeerInfo> GetPeer() const;

    // STATUS accessors
    RelayStatusCode GetStatusCode() const;
    std::string GetStatusText() const;
    std::optional<ReservationInfo> GetReservation() const;

private:
    explicit RelayMessageWrapper(std::unique_ptr<relay::v2::CircuitRelay> msg);

    std::unique_ptr<relay::v2::CircuitRelay> message_;
};

}  // namespace protocol
}  // namespace p2p
