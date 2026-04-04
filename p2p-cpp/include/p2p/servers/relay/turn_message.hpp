/**
 * @file turn_message.hpp
 * @brief TURN Protocol Messages (RFC 5766)
 *
 * Implements TURN protocol message structures and utilities.
 */

#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <array>

namespace p2p {
namespace relay {

// STUN/TURN Magic Cookie
constexpr uint32_t MAGIC_COOKIE = 0x2112A442;

// STUN Message Types
enum class MessageType : uint16_t {
    BINDING_REQUEST = 0x0001,
    BINDING_RESPONSE = 0x0101,
    BINDING_ERROR_RESPONSE = 0x0111,

    // TURN Methods
    ALLOCATE_REQUEST = 0x0003,
    ALLOCATE_RESPONSE = 0x0103,
    ALLOCATE_ERROR_RESPONSE = 0x0113,

    REFRESH_REQUEST = 0x0004,
    REFRESH_RESPONSE = 0x0104,
    REFRESH_ERROR_RESPONSE = 0x0114,

    SEND_INDICATION = 0x0006,
    DATA_INDICATION = 0x0007,

    CREATE_PERMISSION_REQUEST = 0x0008,
    CREATE_PERMISSION_RESPONSE = 0x0108,
    CREATE_PERMISSION_ERROR_RESPONSE = 0x0118,

    CHANNEL_BIND_REQUEST = 0x0009,
    CHANNEL_BIND_RESPONSE = 0x0109,
    CHANNEL_BIND_ERROR_RESPONSE = 0x0119
};

// STUN Attribute Types
enum class AttributeType : uint16_t {
    MAPPED_ADDRESS = 0x0001,
    USERNAME = 0x0006,
    MESSAGE_INTEGRITY = 0x0008,
    ERROR_CODE = 0x0009,
    UNKNOWN_ATTRIBUTES = 0x000A,
    REALM = 0x0014,
    NONCE = 0x0015,
    XOR_MAPPED_ADDRESS = 0x0020,

    // TURN-specific attributes
    CHANNEL_NUMBER = 0x000C,
    LIFETIME = 0x000D,
    XOR_PEER_ADDRESS = 0x0012,
    DATA = 0x0013,
    XOR_RELAYED_ADDRESS = 0x0016,
    EVEN_PORT = 0x0018,
    REQUESTED_TRANSPORT = 0x0019,
    DONT_FRAGMENT = 0x001A,
    RESERVATION_TOKEN = 0x0022,

    SOFTWARE = 0x8022,
    ALTERNATE_SERVER = 0x8023,
    FINGERPRINT = 0x8028
};

// STUN/TURN Error Codes
enum class ErrorCode : uint16_t {
    TRY_ALTERNATE = 300,
    BAD_REQUEST = 400,
    UNAUTHORIZED = 401,
    UNKNOWN_ATTRIBUTE = 420,
    ALLOCATION_MISMATCH = 437,
    STALE_NONCE = 438,
    WRONG_CREDENTIALS = 441,
    UNSUPPORTED_TRANSPORT_PROTOCOL = 442,
    ALLOCATION_QUOTA_REACHED = 486,
    SERVER_ERROR = 500,
    INSUFFICIENT_CAPACITY = 508
};

// Transport Protocol
enum class TransportProtocol : uint8_t {
    UDP = 17,
    TCP = 6
};

/**
 * @brief STUN Attribute
 */
struct StunAttribute {
    AttributeType type;
    std::vector<uint8_t> value;

    StunAttribute() = default;
    StunAttribute(AttributeType t, std::vector<uint8_t> v)
        : type(t), value(std::move(v)) {}
};

/**
 * @brief STUN Message
 */
struct StunMessage {
    MessageType message_type;
    uint16_t message_length;
    uint32_t magic_cookie;
    std::array<uint8_t, 12> transaction_id;
    std::vector<StunAttribute> attributes;

    StunMessage()
        : message_type(MessageType::BINDING_REQUEST),
          message_length(0),
          magic_cookie(MAGIC_COOKIE) {
        transaction_id.fill(0);
    }

    /**
     * @brief Parse STUN message from bytes
     */
    static std::unique_ptr<StunMessage> Parse(const uint8_t* data, size_t len);

    /**
     * @brief Serialize STUN message to bytes
     */
    std::vector<uint8_t> Serialize() const;

    /**
     * @brief Get attribute by type
     */
    const StunAttribute* GetAttribute(AttributeType type) const;

    /**
     * @brief Add attribute
     */
    void AddAttribute(AttributeType type, std::vector<uint8_t> value);
};

/**
 * @brief Address (IP:Port)
 */
struct Address {
    std::string ip;
    uint16_t port;

    Address() : port(0) {}
    Address(std::string ip_, uint16_t port_)
        : ip(std::move(ip_)), port(port_) {}

    bool operator==(const Address& other) const {
        return ip == other.ip && port == other.port;
    }

    bool operator<(const Address& other) const {
        if (ip != other.ip) return ip < other.ip;
        return port < other.port;
    }

    std::string ToString() const {
        return ip + ":" + std::to_string(port);
    }
};

/**
 * @brief Create XOR-MAPPED-ADDRESS attribute
 */
std::vector<uint8_t> CreateXorAddressAttr(
    const Address& addr,
    const std::array<uint8_t, 12>& transaction_id);

/**
 * @brief Parse XOR-MAPPED-ADDRESS attribute
 */
Address ParseXorAddressAttr(
    const std::vector<uint8_t>& data,
    const std::array<uint8_t, 12>& transaction_id);

/**
 * @brief Create LIFETIME attribute
 */
std::vector<uint8_t> CreateLifetimeAttr(uint32_t lifetime);

/**
 * @brief Parse LIFETIME attribute
 */
uint32_t ParseLifetimeAttr(const std::vector<uint8_t>& data);

/**
 * @brief Create ERROR-CODE attribute
 */
std::vector<uint8_t> CreateErrorCodeAttr(ErrorCode code, const std::string& reason);

/**
 * @brief Create error response
 */
std::unique_ptr<StunMessage> CreateErrorResponse(
    const std::array<uint8_t, 12>& transaction_id,
    ErrorCode code,
    const std::string& reason);

} // namespace relay
} // namespace p2p
