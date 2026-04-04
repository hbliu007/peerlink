#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace signaling {

using json = nlohmann::json;

// Forward declarations
class WebSocketSession;

// Message types
enum class MessageType {
    REGISTER,
    REGISTERED,
    UNREGISTER,
    CONNECT,
    CONNECT_REQUEST,
    CONNECT_RESPONSE,
    DISCONNECT,
    OFFER,
    ANSWER,
    ICE_CANDIDATE,
    HEARTBEAT,
    HEARTBEAT_ACK,
    ERROR,
    PING,
    PONG,
    QUERY_DEVICE,
    DEVICE_INFO,
    RELAY_REQUEST,
    RELAY_RESPONSE,
    SERVICE_PUBLISH,
    SERVICE_PUBLISHED,
    SERVICE_UNPUBLISH,
    SERVICE_QUERY,
    SERVICE_INFO,
    SERVICE_CONNECT,
    UNKNOWN
};

// Connection status
enum class ConnectionStatus {
    CONNECTING,
    CONNECTED,
    DISCONNECTED,
    FAILED
};

// NAT types
enum class NATType {
    PUBLIC,
    FULL_CONE,
    RESTRICTED_CONE,
    PORT_RESTRICTED,
    SYMMETRIC,
    UNKNOWN
};

// Error codes
enum class ErrorCode {
    INVALID_REQUEST,
    UNAUTHORIZED,
    DEVICE_NOT_FOUND,
    DEVICE_ALREADY_REGISTERED,
    CONNECTION_FAILED,
    TIMEOUT,
    INTERNAL_ERROR,
    UNSUPPORTED_CAPABILITY,
    RATE_LIMIT_EXCEEDED
};

// Device information
struct DeviceInfo {
    std::string device_id;
    std::shared_ptr<WebSocketSession> session;
    std::string public_key;
    std::vector<std::string> capabilities;
    NATType nat_type = NATType::UNKNOWN;
    std::optional<std::string> public_ip;
    std::optional<int> public_port;
    std::chrono::system_clock::time_point connected_at;
    std::chrono::system_clock::time_point last_heartbeat;
    ConnectionStatus status = ConnectionStatus::CONNECTED;
    json metadata;

    json to_json() const;
};

struct PublishedService {
    std::string name;
    std::string owner_device_id;
    bool relay_only = false;
    std::vector<std::string> capabilities;
    std::vector<std::string> allowed_callers;
    json metadata;
    std::chrono::system_clock::time_point updated_at;

    json to_json() const;
};

// Connection session between two devices
struct ConnectionSession {
    std::string session_id;
    std::string device_a;
    std::string device_b;
    ConnectionStatus status = ConnectionStatus::CONNECTING;
    std::chrono::system_clock::time_point created_at;
    std::optional<std::string> offer;
    std::optional<std::string> answer;
    std::vector<json> ice_candidates_a;
    std::vector<json> ice_candidates_b;
    bool use_relay = false;

    json to_json() const;
};

// Message structure
struct Message {
    MessageType type;
    json data;
    std::chrono::system_clock::time_point timestamp;
    std::optional<std::string> source_device_id;
    std::optional<std::string> target_device_id;
    std::optional<std::string> request_id;

    json to_json() const;
    static Message from_json(const json& j);
};

// Error response
struct ErrorResponse {
    ErrorCode code;
    std::string message;
    std::optional<std::string> request_id;

    json to_json() const;
};

// Utility functions
std::string to_string(MessageType type);
MessageType message_type_from_string(const std::string& str);
std::string to_string(ConnectionStatus status);
std::string to_string(NATType nat_type);
std::string to_string(ErrorCode code);

} // namespace signaling
