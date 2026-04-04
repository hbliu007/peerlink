/**
 * @file turn_message.cpp
 * @brief TURN Message Implementation
 */

#include "p2p/servers/relay/turn_message.hpp"
#include <cstring>
#include <arpa/inet.h>

namespace p2p {
namespace relay {

constexpr size_t MAX_ATTR_LENGTH = 65535;  // TURN 最大属性长度

std::unique_ptr<StunMessage> StunMessage::Parse(const uint8_t* data, size_t len) {
    if (len < 20) {
        return nullptr;
    }

    auto msg = std::make_unique<StunMessage>();

    // Parse header
    uint16_t msg_type;
    std::memcpy(&msg_type, data, 2);
    msg->message_type = static_cast<MessageType>(ntohs(msg_type));

    uint16_t msg_len;
    std::memcpy(&msg_len, data + 2, 2);
    msg->message_length = ntohs(msg_len);

    // Validate message length: buffer must contain at least header + message_length bytes
    // Check len >= 20 + msg->message_length to avoid unsigned underflow
    if (len < 20 + static_cast<size_t>(msg->message_length)) {
        return nullptr;
    }

    uint32_t cookie;
    std::memcpy(&cookie, data + 4, 4);
    msg->magic_cookie = ntohl(cookie);

    if (msg->magic_cookie != MAGIC_COOKIE) {
        return nullptr;
    }

    std::memcpy(msg->transaction_id.data(), data + 8, 12);

    // Parse attributes
    size_t offset = 20;
    size_t remaining = msg->message_length;

    while (remaining > 0 && offset + 4 <= len) {
        uint16_t attr_type, attr_len;
        std::memcpy(&attr_type, data + offset, 2);
        std::memcpy(&attr_len, data + offset + 2, 2);
        attr_type = ntohs(attr_type);
        attr_len = ntohs(attr_len);

        // 验证属性长度
        if (attr_len > MAX_ATTR_LENGTH || attr_len > remaining) {
            return nullptr;  // 返回错误而非继续
        }

        offset += 4;

        // Calculate padded length (align to 4 bytes)
        size_t padded_len = (attr_len + 3) & ~3;

        // 防止整数溢出
        if (padded_len < attr_len || offset + padded_len > len) {
            return nullptr;
        }

        // 正确更新 remaining
        if (remaining < 4 + padded_len) {
            return nullptr;
        }

        std::vector<uint8_t> attr_value(data + offset, data + offset + attr_len);
        msg->attributes.emplace_back(static_cast<AttributeType>(attr_type), std::move(attr_value));

        offset += padded_len;
        remaining -= (4 + padded_len);
    }

    return msg;
}

std::vector<uint8_t> StunMessage::Serialize() const {
    // Serialize attributes first to calculate message length
    std::vector<uint8_t> attr_data;
    for (const auto& attr : attributes) {
        uint16_t type = htons(static_cast<uint16_t>(attr.type));
        uint16_t len = htons(static_cast<uint16_t>(attr.value.size()));

        attr_data.insert(attr_data.end(),
                        reinterpret_cast<const uint8_t*>(&type),
                        reinterpret_cast<const uint8_t*>(&type) + 2);
        attr_data.insert(attr_data.end(),
                        reinterpret_cast<const uint8_t*>(&len),
                        reinterpret_cast<const uint8_t*>(&len) + 2);
        attr_data.insert(attr_data.end(), attr.value.begin(), attr.value.end());

        // Add padding
        size_t padding = (4 - (attr.value.size() % 4)) % 4;
        attr_data.insert(attr_data.end(), padding, 0);
    }

    // Build header
    std::vector<uint8_t> result;
    result.reserve(20 + attr_data.size());

    uint16_t msg_type = htons(static_cast<uint16_t>(message_type));
    uint16_t msg_len = htons(static_cast<uint16_t>(attr_data.size()));
    uint32_t cookie = htonl(magic_cookie);

    result.insert(result.end(),
                 reinterpret_cast<const uint8_t*>(&msg_type),
                 reinterpret_cast<const uint8_t*>(&msg_type) + 2);
    result.insert(result.end(),
                 reinterpret_cast<const uint8_t*>(&msg_len),
                 reinterpret_cast<const uint8_t*>(&msg_len) + 2);
    result.insert(result.end(),
                 reinterpret_cast<const uint8_t*>(&cookie),
                 reinterpret_cast<const uint8_t*>(&cookie) + 4);
    result.insert(result.end(), transaction_id.begin(), transaction_id.end());
    result.insert(result.end(), attr_data.begin(), attr_data.end());

    return result;
}

const StunAttribute* StunMessage::GetAttribute(AttributeType type) const {
    for (const auto& attr : attributes) {
        if (attr.type == type) {
            return &attr;
        }
    }
    return nullptr;
}

void StunMessage::AddAttribute(AttributeType type, std::vector<uint8_t> value) {
    attributes.emplace_back(type, std::move(value));
}

std::vector<uint8_t> CreateXorAddressAttr(
    const Address& addr,
    const std::array<uint8_t, 12>& transaction_id [[maybe_unused]]) {

    std::vector<uint8_t> result;

    // Family (IPv4 = 0x01)
    result.push_back(0);
    result.push_back(0x01);

    // XOR port with (magic_cookie >> 16)
    uint16_t xor_port = addr.port ^ ((MAGIC_COOKIE >> 16) & 0xFFFF);
    uint16_t xor_port_net = htons(xor_port);
    result.insert(result.end(),
                 reinterpret_cast<const uint8_t*>(&xor_port_net),
                 reinterpret_cast<const uint8_t*>(&xor_port_net) + 2);

    // Parse IP address
    uint32_t ip_addr;
    inet_pton(AF_INET, addr.ip.c_str(), &ip_addr);

    // XOR IP with magic cookie
    uint32_t xor_ip = ip_addr ^ htonl(MAGIC_COOKIE);
    result.insert(result.end(),
                 reinterpret_cast<const uint8_t*>(&xor_ip),
                 reinterpret_cast<const uint8_t*>(&xor_ip) + 4);

    return result;
}

Address ParseXorAddressAttr(
    const std::vector<uint8_t>& data,
    const std::array<uint8_t, 12>& transaction_id [[maybe_unused]]) {

    if (data.size() < 8) {
        return Address();
    }

    // Parse XOR port
    uint16_t xor_port_net;
    std::memcpy(&xor_port_net, data.data() + 2, 2);
    uint16_t xor_port = ntohs(xor_port_net);
    uint16_t port = xor_port ^ ((MAGIC_COOKIE >> 16) & 0xFFFF);

    // Parse XOR IP
    uint32_t xor_ip;
    std::memcpy(&xor_ip, data.data() + 4, 4);
    uint32_t ip_addr = xor_ip ^ htonl(MAGIC_COOKIE);

    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ip_addr, ip_str, INET_ADDRSTRLEN);

    return Address(ip_str, port);
}

std::vector<uint8_t> CreateLifetimeAttr(uint32_t lifetime) {
    uint32_t lifetime_net = htonl(lifetime);
    return std::vector<uint8_t>(
        reinterpret_cast<const uint8_t*>(&lifetime_net),
        reinterpret_cast<const uint8_t*>(&lifetime_net) + 4);
}

uint32_t ParseLifetimeAttr(const std::vector<uint8_t>& data) {
    if (data.size() < 4) {
        return 0;
    }
    uint32_t lifetime_net;
    std::memcpy(&lifetime_net, data.data(), 4);
    return ntohl(lifetime_net);
}

std::vector<uint8_t> CreateErrorCodeAttr(ErrorCode code, const std::string& reason) {
    std::vector<uint8_t> result;

    uint16_t error_code = static_cast<uint16_t>(code);
    uint8_t class_digit = error_code / 100;
    uint8_t number = error_code % 100;

    result.push_back(0);
    result.push_back(0);
    result.push_back(class_digit);
    result.push_back(number);

    result.insert(result.end(), reason.begin(), reason.end());

    return result;
}

std::unique_ptr<StunMessage> CreateErrorResponse(
    const std::array<uint8_t, 12>& transaction_id,
    ErrorCode code,
    const std::string& reason) {

    auto msg = std::make_unique<StunMessage>();
    msg->message_type = MessageType::BINDING_ERROR_RESPONSE;
    msg->magic_cookie = MAGIC_COOKIE;
    msg->transaction_id = transaction_id;

    msg->AddAttribute(AttributeType::ERROR_CODE, CreateErrorCodeAttr(code, reason));

    return msg;
}

} // namespace relay
} // namespace p2p
