#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace p2p::protocol {

// STUN Magic Cookie (RFC 5389)
constexpr uint32_t STUN_MAGIC_COOKIE = 0x2112A442;

// STUN Message Types (RFC 5389)
enum class StunMessageType : uint16_t {
    BindingRequest = 0x0001,
    BindingResponse = 0x0101,
    BindingErrorResponse = 0x0111,
};

// STUN Attribute Types (RFC 5389)
enum class StunAttributeType : uint16_t {
    MappedAddress = 0x0001,
    XorMappedAddress = 0x0020,
    ErrorCode = 0x0009,
    UnknownAttributes = 0x000A,
    Software = 0x8022,
    AlternateServer = 0x8023,
    Fingerprint = 0x8028,
};

// STUN Error Codes
enum class StunErrorCode : uint16_t {
    TryAlternate = 300,
    BadRequest = 400,
    Unauthorized = 401,
    UnknownAttribute = 420,
    StaleNonce = 438,
    ServerError = 500,
};

// Address Family
enum class AddressFamily : uint8_t {
    IPv4 = 0x01,
    IPv6 = 0x02,
};

// STUN Transaction ID (96 bits)
using TransactionId = std::array<uint8_t, 12>;

// STUN Attribute
struct StunAttribute {
    StunAttributeType type;
    std::vector<uint8_t> value;

    StunAttribute(StunAttributeType t, std::vector<uint8_t> v)
        : type(t), value(std::move(v)) {}
};

// STUN Message
class StunMessage {
public:
    StunMessage(StunMessageType type, TransactionId tid)
        : message_type_(type), transaction_id_(tid) {}

    // Getters
    StunMessageType message_type() const { return message_type_; }
    const TransactionId& transaction_id() const { return transaction_id_; }
    const std::vector<StunAttribute>& attributes() const { return attributes_; }

    // Add attribute
    void add_attribute(StunAttribute attr) {
        attributes_.push_back(std::move(attr));
    }

    // Get attribute by type
    std::optional<const StunAttribute*> get_attribute(StunAttributeType type) const {
        for (const auto& attr : attributes_) {
            if (attr.type == type) {
                return &attr;
            }
        }
        return std::nullopt;
    }

    // Serialize to bytes
    std::vector<uint8_t> serialize() const;

    // Parse from bytes
    static std::optional<StunMessage> parse(const uint8_t* data, size_t length);

private:
    StunMessageType message_type_;
    TransactionId transaction_id_;
    std::vector<StunAttribute> attributes_;
};

// Helper: Create XOR-MAPPED-ADDRESS attribute
std::vector<uint8_t> create_xor_mapped_address(
    const std::string& ip,
    uint16_t port,
    const TransactionId& transaction_id
);

// Helper: Parse XOR-MAPPED-ADDRESS attribute
std::optional<std::pair<std::string, uint16_t>> parse_xor_mapped_address(
    const std::vector<uint8_t>& data,
    const TransactionId& transaction_id
);

// Helper: Create error response
StunMessage create_error_response(
    const TransactionId& transaction_id,
    StunErrorCode error_code,
    const std::string& reason
);

}  // namespace p2p::protocol
