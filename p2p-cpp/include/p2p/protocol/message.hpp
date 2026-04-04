#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <optional>

namespace p2p {
namespace protocol {

/**
 * @brief Message type identifiers
 */
enum class MessageType : uint8_t {
    HANDSHAKE = 0x01,
    HANDSHAKE_ACK = 0x02,
    KEEPALIVE = 0x03,
    CHANNEL_DATA = 0x04,
    CHANNEL_OPEN = 0x05,
    CHANNEL_CLOSE = 0x06,
    DISCONNECT = 0x07,
    ERROR = 0x08
};

/**
 * @brief Base P2P message structure
 */
class Message {
public:
    Message(MessageType type,
            const std::string& sender_did,
            const std::string& receiver_did);

    virtual ~Message() = default;

    // Getters
    MessageType type() const { return type_; }
    const std::string& sender_did() const { return sender_did_; }
    const std::string& receiver_did() const { return receiver_did_; }
    const std::string& message_id() const { return message_id_; }
    std::optional<int> channel_id() const { return channel_id_; }
    const std::vector<uint8_t>& payload() const { return payload_; }
    uint64_t timestamp() const { return timestamp_; }
    const std::map<std::string, std::string>& metadata() const { return metadata_; }

    // Setters
    void set_channel_id(int id) { channel_id_ = id; }
    void set_payload(const std::vector<uint8_t>& data) { payload_ = data; }
    void set_payload(std::vector<uint8_t>&& data) { payload_ = std::move(data); }
    void add_metadata(const std::string& key, const std::string& value) {
        metadata_[key] = value;
    }

    /**
     * @brief Encode message to bytes for transmission
     * @return Encoded message bytes
     */
    std::vector<uint8_t> encode() const;

    /**
     * @brief Decode message from bytes
     * @param data Raw message bytes
     * @return Decoded message or nullptr on failure
     */
    static std::unique_ptr<Message> decode(const std::vector<uint8_t>& data);

protected:
    MessageType type_;
    std::string sender_did_;
    std::string receiver_did_;
    std::string message_id_;
    std::optional<int> channel_id_;
    std::vector<uint8_t> payload_;
    uint64_t timestamp_;
    std::map<std::string, std::string> metadata_;

    static std::string generate_message_id();
};

/**
 * @brief Handshake message for connection establishment
 */
class HandshakeMessage : public Message {
public:
    HandshakeMessage(const std::string& sender_did,
                     const std::string& receiver_did,
                     bool is_ack = false);

    void set_public_address(const std::string& ip, uint16_t port);
    void set_local_address(const std::string& ip, uint16_t port);
    void set_nat_type(const std::string& nat_type);
    void add_capability(const std::string& capability);

    std::optional<std::pair<std::string, uint16_t>> public_address() const;
    std::optional<std::pair<std::string, uint16_t>> local_address() const;
    std::optional<std::string> nat_type() const;
    std::vector<std::string> capabilities() const;
    bool is_ack() const { return is_ack_; }

private:
    bool is_ack_;
};

/**
 * @brief Channel data message
 */
class ChannelDataMessage : public Message {
public:
    ChannelDataMessage(const std::string& sender_did,
                       const std::string& receiver_did,
                       int channel_id,
                       const std::vector<uint8_t>& data);
};

/**
 * @brief Keepalive message
 */
class KeepaliveMessage : public Message {
public:
    KeepaliveMessage(const std::string& sender_did,
                     const std::string& receiver_did);
};

/**
 * @brief Disconnect message
 */
class DisconnectMessage : public Message {
public:
    DisconnectMessage(const std::string& sender_did,
                      const std::string& receiver_did,
                      const std::string& reason = "");
};

} // namespace protocol
} // namespace p2p
