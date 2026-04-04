#include "p2p/protocol/stun.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <sstream>

namespace p2p::protocol {

constexpr size_t MAX_ATTR_LENGTH = 65535;  // STUN 最大属性长度

// Serialize STUN message to bytes
std::vector<uint8_t> StunMessage::serialize() const {
    std::vector<uint8_t> result;

    // Serialize attributes first to calculate message length
    std::vector<uint8_t> attr_data;
    for (const auto& attr : attributes_) {
        // Attribute type (2 bytes)
        uint16_t type = htons(static_cast<uint16_t>(attr.type));
        attr_data.insert(attr_data.end(),
                        reinterpret_cast<const uint8_t*>(&type),
                        reinterpret_cast<const uint8_t*>(&type) + 2);

        // Attribute length (2 bytes)
        uint16_t length = htons(static_cast<uint16_t>(attr.value.size()));
        attr_data.insert(attr_data.end(),
                        reinterpret_cast<const uint8_t*>(&length),
                        reinterpret_cast<const uint8_t*>(&length) + 2);

        // Attribute value
        attr_data.insert(attr_data.end(), attr.value.begin(), attr.value.end());

        // Padding to 4-byte boundary
        size_t padding = (4 - (attr.value.size() % 4)) % 4;
        attr_data.insert(attr_data.end(), padding, 0);
    }

    // Message type (2 bytes)
    uint16_t type = htons(static_cast<uint16_t>(message_type_));
    result.insert(result.end(),
                 reinterpret_cast<const uint8_t*>(&type),
                 reinterpret_cast<const uint8_t*>(&type) + 2);

    // Message length (2 bytes) - length of attributes only
    uint16_t length = htons(static_cast<uint16_t>(attr_data.size()));
    result.insert(result.end(),
                 reinterpret_cast<const uint8_t*>(&length),
                 reinterpret_cast<const uint8_t*>(&length) + 2);

    // Magic cookie (4 bytes)
    uint32_t cookie = htonl(STUN_MAGIC_COOKIE);
    result.insert(result.end(),
                 reinterpret_cast<const uint8_t*>(&cookie),
                 reinterpret_cast<const uint8_t*>(&cookie) + 4);

    // Transaction ID (12 bytes)
    result.insert(result.end(), transaction_id_.begin(), transaction_id_.end());

    // Attributes
    result.insert(result.end(), attr_data.begin(), attr_data.end());

    return result;
}

// Parse STUN message from bytes
std::optional<StunMessage> StunMessage::parse(const uint8_t* data, size_t length) {
    if (length < 20) {
        return std::nullopt;  // Minimum STUN header size
    }

    // Parse header
    uint16_t message_type = ntohs(*reinterpret_cast<const uint16_t*>(data));
    uint16_t message_length = ntohs(*reinterpret_cast<const uint16_t*>(data + 2));
    uint32_t magic_cookie = ntohl(*reinterpret_cast<const uint32_t*>(data + 4));

    if (magic_cookie != STUN_MAGIC_COOKIE) {
        return std::nullopt;
    }

    // Extract transaction ID
    TransactionId transaction_id;
    std::memcpy(transaction_id.data(), data + 8, 12);

    // Create message
    StunMessage message(static_cast<StunMessageType>(message_type), transaction_id);

    // Parse attributes
    size_t offset = 20;
    size_t remaining = message_length;

    while (remaining > 0 && offset + 4 <= length) {
        uint16_t attr_type = ntohs(*reinterpret_cast<const uint16_t*>(data + offset));
        uint16_t attr_length = ntohs(*reinterpret_cast<const uint16_t*>(data + offset + 2));

        // 验证属性长度
        if (attr_length > MAX_ATTR_LENGTH || attr_length > remaining) {
            return std::nullopt;  // 返回错误而非继续
        }

        offset += 4;

        // Calculate padded length (4-byte boundary)
        size_t padded_length = (attr_length + 3) & ~3;

        // 防止整数溢出
        if (padded_length < attr_length || offset + padded_length > length) {
            return std::nullopt;
        }

        // 正确更新 remaining
        if (remaining < 4 + padded_length) {
            return std::nullopt;
        }

        // Extract attribute value
        std::vector<uint8_t> attr_value(data + offset, data + offset + attr_length);
        message.add_attribute(StunAttribute(
            static_cast<StunAttributeType>(attr_type),
            std::move(attr_value)
        ));

        offset += padded_length;
        remaining -= (4 + padded_length);
    }

    return message;
}

// Create XOR-MAPPED-ADDRESS attribute
std::vector<uint8_t> create_xor_mapped_address(
    const std::string& ip,
    uint16_t port,
    const TransactionId& transaction_id
) {
    std::vector<uint8_t> result;

    // Determine address family
    bool is_ipv4 = ip.find('.') != std::string::npos;
    AddressFamily family = is_ipv4 ? AddressFamily::IPv4 : AddressFamily::IPv6;

    // Reserved byte
    result.push_back(0);

    // Family
    result.push_back(static_cast<uint8_t>(family));

    // XOR port with (magic_cookie >> 16)
    uint16_t xor_port = port ^ ((STUN_MAGIC_COOKIE >> 16) & 0xFFFF);
    uint16_t xor_port_net = htons(xor_port);
    result.insert(result.end(),
                 reinterpret_cast<const uint8_t*>(&xor_port_net),
                 reinterpret_cast<const uint8_t*>(&xor_port_net) + 2);

    // Magic cookie bytes for XOR
    uint32_t cookie_net = htonl(STUN_MAGIC_COOKIE);
    const uint8_t* cookie_bytes = reinterpret_cast<const uint8_t*>(&cookie_net);

    if (is_ipv4) {
        // Parse IPv4 address
        uint32_t ip_addr;
        inet_pton(AF_INET, ip.c_str(), &ip_addr);
        const uint8_t* ip_bytes = reinterpret_cast<const uint8_t*>(&ip_addr);

        // XOR with magic cookie
        for (int i = 0; i < 4; ++i) {
            result.push_back(ip_bytes[i] ^ cookie_bytes[i]);
        }
    } else {
        // Parse IPv6 address
        uint8_t ip_addr[16];
        inet_pton(AF_INET6, ip.c_str(), ip_addr);

        // XOR with magic cookie + transaction ID
        for (int i = 0; i < 16; ++i) {
            uint8_t xor_byte = (i < 4) ? cookie_bytes[i] : transaction_id[i - 4];
            result.push_back(ip_addr[i] ^ xor_byte);
        }
    }

    return result;
}

// Parse XOR-MAPPED-ADDRESS attribute
std::optional<std::pair<std::string, uint16_t>> parse_xor_mapped_address(
    const std::vector<uint8_t>& data,
    const TransactionId& transaction_id
) {
    if (data.size() < 8) {
        return std::nullopt;
    }

    AddressFamily family = static_cast<AddressFamily>(data[1]);

    // XOR port
    uint16_t xor_port = ntohs(*reinterpret_cast<const uint16_t*>(&data[2]));
    uint16_t port = xor_port ^ ((STUN_MAGIC_COOKIE >> 16) & 0xFFFF);

    // Magic cookie bytes
    uint32_t cookie_net = htonl(STUN_MAGIC_COOKIE);
    const uint8_t* cookie_bytes = reinterpret_cast<const uint8_t*>(&cookie_net);

    if (family == AddressFamily::IPv4) {
        if (data.size() < 8) {
            return std::nullopt;
        }

        // XOR IP with magic cookie
        uint8_t ip_bytes[4];
        for (int i = 0; i < 4; ++i) {
            ip_bytes[i] = data[4 + i] ^ cookie_bytes[i];
        }

        // Convert to string
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, ip_bytes, ip_str, INET_ADDRSTRLEN);

        return std::make_pair(std::string(ip_str), port);
    } else if (family == AddressFamily::IPv6) {
        if (data.size() < 20) {
            return std::nullopt;
        }

        // XOR IP with magic cookie + transaction ID
        uint8_t ip_bytes[16];
        for (int i = 0; i < 16; ++i) {
            uint8_t xor_byte = (i < 4) ? cookie_bytes[i] : transaction_id[i - 4];
            ip_bytes[i] = data[4 + i] ^ xor_byte;
        }

        // Convert to string
        char ip_str[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, ip_bytes, ip_str, INET6_ADDRSTRLEN);

        return std::make_pair(std::string(ip_str), port);
    }

    return std::nullopt;
}

// Create error response
StunMessage create_error_response(
    const TransactionId& transaction_id,
    StunErrorCode error_code,
    const std::string& reason
) {
    StunMessage message(StunMessageType::BindingErrorResponse, transaction_id);

    // Error code attribute format:
    // 0x00 0x00 class (hundreds) number
    uint16_t code_value = static_cast<uint16_t>(error_code);
    uint8_t class_digit = code_value / 100;
    uint8_t number = code_value % 100;

    std::vector<uint8_t> error_attr;
    error_attr.push_back(0);  // Reserved
    error_attr.push_back(0);  // Reserved
    error_attr.push_back(class_digit);
    error_attr.push_back(number);

    // Reason phrase
    error_attr.insert(error_attr.end(), reason.begin(), reason.end());

    message.add_attribute(StunAttribute(
        StunAttributeType::ErrorCode,
        std::move(error_attr)
    ));

    return message;
}

}  // namespace p2p::protocol
