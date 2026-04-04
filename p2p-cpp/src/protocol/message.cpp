#include "p2p/protocol/message.hpp"
#include <nlohmann/json.hpp>
#include <random>
#include <sstream>
#include <iomanip>
#include <cstring>

using json = nlohmann::json;

namespace p2p {
namespace protocol {

namespace {
    // Helper to convert timestamp to milliseconds since epoch
    uint64_t current_timestamp_ms() {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }
}

Message::Message(MessageType type,
                 const std::string& sender_did,
                 const std::string& receiver_did)
    : type_(type)
    , sender_did_(sender_did)
    , receiver_did_(receiver_did)
    , message_id_(generate_message_id())
    , timestamp_(current_timestamp_ms())
{
}

std::string Message::generate_message_id() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    for (int i = 0; i < 32; ++i) {
        if (i == 8 || i == 12 || i == 16 || i == 20) {
            ss << '-';
        }
        ss << std::setw(1) << dis(gen);
    }

    return ss.str();
}

std::vector<uint8_t> Message::encode() const {
    // Create JSON object
    json j;
    j["msg_type"] = static_cast<uint8_t>(type_);
    j["sender_did"] = sender_did_;
    j["receiver_did"] = receiver_did_;
    j["message_id"] = message_id_;
    j["timestamp"] = timestamp_;

    if (channel_id_.has_value()) {
        j["channel_id"] = channel_id_.value();
    } else {
        j["channel_id"] = nullptr;
    }

    if (!metadata_.empty()) {
        j["metadata"] = metadata_;
    }

    // Serialize JSON to string
    std::string json_str = j.dump();
    std::vector<uint8_t> json_bytes(json_str.begin(), json_str.end());

    // Format: [total_length(4)][json_length(4)][json_bytes][payload]
    uint32_t total_length = static_cast<uint32_t>(json_bytes.size() + payload_.size());
    uint32_t json_length = static_cast<uint32_t>(json_bytes.size());

    std::vector<uint8_t> result;
    result.reserve(8 + total_length);

    // Write total_length (big-endian)
    result.push_back((total_length >> 24) & 0xFF);
    result.push_back((total_length >> 16) & 0xFF);
    result.push_back((total_length >> 8) & 0xFF);
    result.push_back(total_length & 0xFF);

    // Write json_length (big-endian)
    result.push_back((json_length >> 24) & 0xFF);
    result.push_back((json_length >> 16) & 0xFF);
    result.push_back((json_length >> 8) & 0xFF);
    result.push_back(json_length & 0xFF);

    // Write JSON bytes
    result.insert(result.end(), json_bytes.begin(), json_bytes.end());

    // Write payload
    result.insert(result.end(), payload_.begin(), payload_.end());

    return result;
}

std::unique_ptr<Message> Message::decode(const std::vector<uint8_t>& data) {
    // Input validation: size limits to prevent DoS attacks
    constexpr size_t MAX_MESSAGE_SIZE = 64 * 1024;  // 64KB max message
    constexpr size_t MAX_JSON_SIZE = 16 * 1024;     // 16KB max JSON metadata
    constexpr size_t MAX_PAYLOAD_SIZE = 60 * 1024;  // 60KB max payload
    constexpr size_t MIN_HEADER_SIZE = 8;

    // Validate minimum size
    if (data.size() < MIN_HEADER_SIZE) {
        return nullptr;
    }

    // Validate maximum size to prevent memory exhaustion
    if (data.size() > MAX_MESSAGE_SIZE) {
        return nullptr;
    }

    // Parse header
    uint32_t total_length = (static_cast<uint32_t>(data[0]) << 24) |
                           (static_cast<uint32_t>(data[1]) << 16) |
                           (static_cast<uint32_t>(data[2]) << 8) |
                           static_cast<uint32_t>(data[3]);

    uint32_t json_length = (static_cast<uint32_t>(data[4]) << 24) |
                          (static_cast<uint32_t>(data[5]) << 16) |
                          (static_cast<uint32_t>(data[6]) << 8) |
                          static_cast<uint32_t>(data[7]);

    // Validate header values to prevent integer overflow and DoS
    if (total_length > MAX_MESSAGE_SIZE) {
        return nullptr;
    }

    if (json_length > MAX_JSON_SIZE) {
        return nullptr;
    }

    // Validate that json_length doesn't exceed total_length
    if (json_length > total_length) {
        return nullptr;
    }

    // Validate payload size
    if (total_length > json_length) {
        size_t payload_size = total_length - json_length;
        if (payload_size > MAX_PAYLOAD_SIZE) {
            return nullptr;
        }
    }

    if (data.size() < MIN_HEADER_SIZE + total_length) {
        return nullptr;
    }

    // Parse JSON
    try {
        std::string json_str(data.begin() + MIN_HEADER_SIZE,
                            data.begin() + MIN_HEADER_SIZE + json_length);
        json j = json::parse(json_str);

        // Validate required fields exist
        if (!j.contains("msg_type") || !j.contains("sender_did") ||
            !j.contains("receiver_did") || !j.contains("message_id") ||
            !j.contains("timestamp")) {
            return nullptr;
        }

        // Validate field types
        if (!j["msg_type"].is_number() || !j["sender_did"].is_string() ||
            !j["receiver_did"].is_string() || !j["message_id"].is_string() ||
            !j["timestamp"].is_number()) {
            return nullptr;
        }

        MessageType msg_type = static_cast<MessageType>(j["msg_type"].get<uint8_t>());

        // Validate message type is in valid range
        if (static_cast<uint8_t>(msg_type) < 0x01 ||
            static_cast<uint8_t>(msg_type) > 0x08) {
            return nullptr;
        }

        std::string sender_did = j["sender_did"].get<std::string>();
        std::string receiver_did = j["receiver_did"].get<std::string>();

        // Validate DID lengths (reasonable limits)
        constexpr size_t MAX_DID_LENGTH = 256;
        if (sender_did.empty() || sender_did.length() > MAX_DID_LENGTH ||
            receiver_did.empty() || receiver_did.length() > MAX_DID_LENGTH) {
            return nullptr;
        }

        auto msg = std::make_unique<Message>(msg_type, sender_did, receiver_did);

        msg->message_id_ = j["message_id"].get<std::string>();

        // Validate message_id length
        constexpr size_t MAX_MESSAGE_ID_LENGTH = 64;
        if (msg->message_id_.empty() || msg->message_id_.length() > MAX_MESSAGE_ID_LENGTH) {
            return nullptr;
        }

        msg->timestamp_ = j["timestamp"].get<uint64_t>();

        // Validate timestamp is reasonable (not too far in future/past)
        auto now = std::chrono::system_clock::now();
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        constexpr uint64_t MAX_TIME_DRIFT_MS = 24 * 60 * 60 * 1000;  // 24 hours

        if (msg->timestamp_ > static_cast<uint64_t>(now_ms + MAX_TIME_DRIFT_MS)) {
            return nullptr;  // Timestamp too far in future
        }

        if (!j["channel_id"].is_null()) {
            if (!j["channel_id"].is_number()) {
                return nullptr;
            }
            int channel_id = j["channel_id"].get<int>();
            // Validate channel_id is in reasonable range
            if (channel_id < 0 || channel_id > 65535) {
                return nullptr;
            }
            msg->channel_id_ = channel_id;
        }

        if (j.contains("metadata") && j["metadata"].is_object()) {
            auto metadata = j["metadata"].get<std::map<std::string, std::string>>();

            // Validate metadata size
            constexpr size_t MAX_METADATA_ENTRIES = 32;
            constexpr size_t MAX_METADATA_KEY_LENGTH = 128;
            constexpr size_t MAX_METADATA_VALUE_LENGTH = 1024;

            if (metadata.size() > MAX_METADATA_ENTRIES) {
                return nullptr;
            }

            for (const auto& [key, value] : metadata) {
                if (key.empty() || key.length() > MAX_METADATA_KEY_LENGTH ||
                    value.length() > MAX_METADATA_VALUE_LENGTH) {
                    return nullptr;
                }
            }

            msg->metadata_ = std::move(metadata);
        }

        // Extract payload — use total_length to bound extraction,
        // not data.end(), which may include bytes from the next message
        size_t payload_length = total_length - json_length;
        if (payload_length > 0) {
            size_t payload_start = MIN_HEADER_SIZE + json_length;
            msg->payload_.assign(data.begin() + payload_start,
                                 data.begin() + payload_start + payload_length);
        }

        return msg;
    } catch (const std::exception&) {
        return nullptr;
    }
}

// HandshakeMessage implementation
HandshakeMessage::HandshakeMessage(const std::string& sender_did,
                                   const std::string& receiver_did,
                                   bool is_ack)
    : Message(is_ack ? MessageType::HANDSHAKE_ACK : MessageType::HANDSHAKE,
              sender_did, receiver_did)
    , is_ack_(is_ack)
{
    add_metadata("is_ack", is_ack ? "true" : "false");
}

void HandshakeMessage::set_public_address(const std::string& ip, uint16_t port) {
    add_metadata("public_ip", ip);
    add_metadata("public_port", std::to_string(port));
}

void HandshakeMessage::set_local_address(const std::string& ip, uint16_t port) {
    add_metadata("local_ip", ip);
    add_metadata("local_port", std::to_string(port));
}

void HandshakeMessage::set_nat_type(const std::string& nat_type) {
    add_metadata("nat_type", nat_type);
}

void HandshakeMessage::add_capability(const std::string& capability) {
    // Store capabilities as comma-separated list
    auto it = metadata_.find("capabilities");
    if (it != metadata_.end()) {
        it->second += "," + capability;
    } else {
        add_metadata("capabilities", capability);
    }
}

std::optional<std::pair<std::string, uint16_t>> HandshakeMessage::public_address() const {
    auto ip_it = metadata_.find("public_ip");
    auto port_it = metadata_.find("public_port");
    if (ip_it != metadata_.end() && port_it != metadata_.end()) {
        return std::make_pair(ip_it->second, static_cast<uint16_t>(std::stoi(port_it->second)));
    }
    return std::nullopt;
}

std::optional<std::pair<std::string, uint16_t>> HandshakeMessage::local_address() const {
    auto ip_it = metadata_.find("local_ip");
    auto port_it = metadata_.find("local_port");
    if (ip_it != metadata_.end() && port_it != metadata_.end()) {
        return std::make_pair(ip_it->second, static_cast<uint16_t>(std::stoi(port_it->second)));
    }
    return std::nullopt;
}

std::optional<std::string> HandshakeMessage::nat_type() const {
    auto it = metadata_.find("nat_type");
    if (it != metadata_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<std::string> HandshakeMessage::capabilities() const {
    auto it = metadata_.find("capabilities");
    if (it == metadata_.end()) {
        return {};
    }

    std::vector<std::string> result;
    std::stringstream ss(it->second);
    std::string item;
    while (std::getline(ss, item, ',')) {
        result.push_back(item);
    }
    return result;
}

// ChannelDataMessage implementation
ChannelDataMessage::ChannelDataMessage(const std::string& sender_did,
                                       const std::string& receiver_did,
                                       int channel_id,
                                       const std::vector<uint8_t>& data)
    : Message(MessageType::CHANNEL_DATA, sender_did, receiver_did)
{
    set_channel_id(channel_id);
    set_payload(data);
}

// KeepaliveMessage implementation
KeepaliveMessage::KeepaliveMessage(const std::string& sender_did,
                                   const std::string& receiver_did)
    : Message(MessageType::KEEPALIVE, sender_did, receiver_did)
{
}

// DisconnectMessage implementation
DisconnectMessage::DisconnectMessage(const std::string& sender_did,
                                     const std::string& receiver_did,
                                     const std::string& reason)
    : Message(MessageType::DISCONNECT, sender_did, receiver_did)
{
    if (!reason.empty()) {
        add_metadata("reason", reason);
    }
}

} // namespace protocol
} // namespace p2p
