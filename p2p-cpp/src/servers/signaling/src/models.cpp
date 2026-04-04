#include "models.hpp"
#include <chrono>
#include <unordered_map>

namespace signaling {

// MessageType conversion
std::string to_string(MessageType type) {
    static const std::unordered_map<MessageType, std::string> map = {
        {MessageType::REGISTER, "register"},
        {MessageType::REGISTERED, "registered"},
        {MessageType::UNREGISTER, "unregister"},
        {MessageType::CONNECT, "connect"},
        {MessageType::CONNECT_REQUEST, "connect_request"},
        {MessageType::CONNECT_RESPONSE, "connect_response"},
        {MessageType::DISCONNECT, "disconnect"},
        {MessageType::OFFER, "offer"},
        {MessageType::ANSWER, "answer"},
        {MessageType::ICE_CANDIDATE, "ice_candidate"},
        {MessageType::HEARTBEAT, "heartbeat"},
        {MessageType::HEARTBEAT_ACK, "heartbeat_ack"},
        {MessageType::ERROR, "error"},
        {MessageType::PING, "ping"},
        {MessageType::PONG, "pong"},
        {MessageType::QUERY_DEVICE, "query_device"},
        {MessageType::DEVICE_INFO, "device_info"},
        {MessageType::RELAY_REQUEST, "relay_request"},
        {MessageType::RELAY_RESPONSE, "relay_response"},
        {MessageType::SERVICE_PUBLISH, "service_publish"},
        {MessageType::SERVICE_PUBLISHED, "service_published"},
        {MessageType::SERVICE_UNPUBLISH, "service_unpublish"},
        {MessageType::SERVICE_QUERY, "service_query"},
        {MessageType::SERVICE_INFO, "service_info"},
        {MessageType::SERVICE_CONNECT, "service_connect"},
    };
    auto it = map.find(type);
    return it != map.end() ? it->second : "unknown";
}

MessageType message_type_from_string(const std::string& str) {
    static const std::unordered_map<std::string, MessageType> map = {
        {"register", MessageType::REGISTER},
        {"registered", MessageType::REGISTERED},
        {"unregister", MessageType::UNREGISTER},
        {"connect", MessageType::CONNECT},
        {"connect_request", MessageType::CONNECT_REQUEST},
        {"connect_response", MessageType::CONNECT_RESPONSE},
        {"disconnect", MessageType::DISCONNECT},
        {"offer", MessageType::OFFER},
        {"answer", MessageType::ANSWER},
        {"ice_candidate", MessageType::ICE_CANDIDATE},
        {"heartbeat", MessageType::HEARTBEAT},
        {"heartbeat_ack", MessageType::HEARTBEAT_ACK},
        {"error", MessageType::ERROR},
        {"ping", MessageType::PING},
        {"pong", MessageType::PONG},
        {"query_device", MessageType::QUERY_DEVICE},
        {"device_info", MessageType::DEVICE_INFO},
        {"relay_request", MessageType::RELAY_REQUEST},
        {"relay_response", MessageType::RELAY_RESPONSE},
        {"service_publish", MessageType::SERVICE_PUBLISH},
        {"service_published", MessageType::SERVICE_PUBLISHED},
        {"service_unpublish", MessageType::SERVICE_UNPUBLISH},
        {"service_query", MessageType::SERVICE_QUERY},
        {"service_info", MessageType::SERVICE_INFO},
        {"service_connect", MessageType::SERVICE_CONNECT},
    };
    auto it = map.find(str);
    return it != map.end() ? it->second : MessageType::UNKNOWN;
}

// ConnectionStatus conversion
std::string to_string(ConnectionStatus status) {
    switch (status) {
        case ConnectionStatus::CONNECTING: return "connecting";
        case ConnectionStatus::CONNECTED: return "connected";
        case ConnectionStatus::DISCONNECTED: return "disconnected";
        case ConnectionStatus::FAILED: return "failed";
    }
    return "unknown";
}

// NATType conversion
std::string to_string(NATType nat_type) {
    switch (nat_type) {
        case NATType::PUBLIC: return "public";
        case NATType::FULL_CONE: return "full_cone";
        case NATType::RESTRICTED_CONE: return "restricted_cone";
        case NATType::PORT_RESTRICTED: return "port_restricted";
        case NATType::SYMMETRIC: return "symmetric";
        case NATType::UNKNOWN: return "unknown";
    }
    return "unknown";
}

// ErrorCode conversion
std::string to_string(ErrorCode code) {
    switch (code) {
        case ErrorCode::INVALID_REQUEST: return "INVALID_REQUEST";
        case ErrorCode::UNAUTHORIZED: return "UNAUTHORIZED";
        case ErrorCode::DEVICE_NOT_FOUND: return "DEVICE_NOT_FOUND";
        case ErrorCode::DEVICE_ALREADY_REGISTERED: return "DEVICE_ALREADY_REGISTERED";
        case ErrorCode::CONNECTION_FAILED: return "CONNECTION_FAILED";
        case ErrorCode::TIMEOUT: return "TIMEOUT";
        case ErrorCode::INTERNAL_ERROR: return "INTERNAL_ERROR";
        case ErrorCode::UNSUPPORTED_CAPABILITY: return "UNSUPPORTED_CAPABILITY";
        case ErrorCode::RATE_LIMIT_EXCEEDED: return "RATE_LIMIT_EXCEEDED";
    }
    return "UNKNOWN";
}

// DeviceInfo serialization
json DeviceInfo::to_json() const {
    auto timestamp_to_int = [](const auto& tp) {
        return std::chrono::duration_cast<std::chrono::seconds>(
            tp.time_since_epoch()
        ).count();
    };

    json j = {
        {"device_id", device_id},
        {"public_key", public_key},
        {"capabilities", capabilities},
        {"nat_type", signaling::to_string(nat_type)},
        {"connected_at", timestamp_to_int(connected_at)},
        {"last_heartbeat", timestamp_to_int(last_heartbeat)},
        {"status", signaling::to_string(status)},
        {"metadata", metadata}
    };

    if (public_ip) j["public_ip"] = *public_ip;
    if (public_port) j["public_port"] = *public_port;

    return j;
}

json PublishedService::to_json() const {
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        updated_at.time_since_epoch()).count();

    return {
        {"name", name},
        {"owner_device_id", owner_device_id},
        {"relay_only", relay_only},
        {"capabilities", capabilities},
        {"allowed_callers", allowed_callers},
        {"metadata", metadata},
        {"updated_at", timestamp}
    };
}

// ConnectionSession serialization
json ConnectionSession::to_json() const {
    auto timestamp_to_int = [](const auto& tp) {
        return std::chrono::duration_cast<std::chrono::seconds>(
            tp.time_since_epoch()
        ).count();
    };

    return {
        {"session_id", session_id},
        {"device_a", device_a},
        {"device_b", device_b},
        {"status", signaling::to_string(status)},
        {"created_at", timestamp_to_int(created_at)},
        {"use_relay", use_relay}
    };
}

// Message serialization
json Message::to_json() const {
    auto timestamp_to_int = [](const auto& tp) {
        return std::chrono::duration_cast<std::chrono::seconds>(
            tp.time_since_epoch()
        ).count();
    };

    json j = {
        {"type", signaling::to_string(type)},
        {"data", data},
        {"timestamp", timestamp_to_int(timestamp)}
    };

    if (source_device_id) j["source_device_id"] = *source_device_id;
    if (target_device_id) j["target_device_id"] = *target_device_id;
    if (request_id) j["request_id"] = *request_id;

    return j;
}

Message Message::from_json(const json& j) {
    Message msg;
    msg.type = message_type_from_string(j.value("type", "unknown"));
    msg.data = j.value("data", json::object());

    if (j.contains("timestamp")) {
        auto ts = j["timestamp"].get<int64_t>();
        msg.timestamp = std::chrono::system_clock::time_point(
            std::chrono::seconds(ts)
        );
    } else {
        msg.timestamp = std::chrono::system_clock::now();
    }

    if (j.contains("source_device_id")) {
        msg.source_device_id = j["source_device_id"].get<std::string>();
    }
    if (j.contains("target_device_id")) {
        msg.target_device_id = j["target_device_id"].get<std::string>();
    }
    if (j.contains("request_id")) {
        msg.request_id = j["request_id"].get<std::string>();
    }

    return msg;
}

// ErrorResponse serialization
json ErrorResponse::to_json() const {
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    json j = {
        {"type", "error"},
        {"data", {
            {"code", signaling::to_string(code)},
            {"message", message}
        }},
        {"timestamp", now}
    };

    if (request_id) {
        j["request_id"] = *request_id;
        j["data"]["request_id"] = *request_id;
    }

    return j;
}

} // namespace signaling