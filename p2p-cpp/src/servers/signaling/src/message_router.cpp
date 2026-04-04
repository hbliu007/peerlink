#include "message_router.hpp"
#include "p2p/utils/logger.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <limits>

namespace asio = boost::asio;

namespace signaling {
namespace {

constexpr char kRelayPublicHostEnv[] = "RELAY_PUBLIC_HOST";
constexpr char kRelayPublicPortEnv[] = "RELAY_PUBLIC_PORT";
constexpr char kTurnPublicHostEnv[] = "TURN_PUBLIC_IP";
constexpr char kTurnPortEnv[] = "TURN_PORT";
constexpr char kAllowInsecureRegistrationEnv[] = "SIGNALING_ALLOW_INSECURE_REGISTRATION";
constexpr char kDefaultRelayPublicHost[] = "relay.peerlink.example.org";
constexpr uint16_t kDefaultRelayPublicPort = 50000;

const char* GetEnvValue(const char* name) {
    const char* value = std::getenv(name);
    return (value != nullptr && value[0] != '\0') ? value : nullptr;
}

uint16_t GetEnvPortOrDefault(const char* name, uint16_t default_value) {
    const char* value = GetEnvValue(name);
    if (value == nullptr) {
        return default_value;
    }

    errno = 0;
    char* end = nullptr;
    long parsed = std::strtol(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' ||
        parsed < 0 || parsed > std::numeric_limits<uint16_t>::max()) {
        return default_value;
    }

    return static_cast<uint16_t>(parsed);
}

bool GetEnvBool(const char* name, bool default_value) {
    const char* value = GetEnvValue(name);
    if (value == nullptr) {
        return default_value;
    }

    std::string normalized(value);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (normalized == "1" || normalized == "true" ||
        normalized == "yes" || normalized == "on") {
        return true;
    }
    if (normalized == "0" || normalized == "false" ||
        normalized == "no" || normalized == "off") {
        return false;
    }
    return default_value;
}

bool IsTemporaryDeviceId(const std::string& device_id) {
    return device_id.rfind("temp_", 0) == 0;
}

std::optional<json> RequireRegisteredDevice(const std::shared_ptr<ConnectionManager>& manager,
                                            const std::string& device_id,
                                            const std::optional<std::string>& request_id) {
    auto device = manager->get_device(device_id);
    if (!device.has_value() || IsTemporaryDeviceId(device_id)) {
        return ErrorResponse{
            ErrorCode::UNAUTHORIZED,
            "device must complete register before using signaling operations",
            request_id
        }.to_json();
    }

    if (!GetEnvBool(kAllowInsecureRegistrationEnv, false) &&
        device->metadata.value("auth_mode", "") != "jwt") {
        return ErrorResponse{
            ErrorCode::UNAUTHORIZED,
            "device must present a valid auth_token in secure mode",
            request_id
        }.to_json();
    }

    return std::nullopt;
}

bool IsSessionMember(const ConnectionSession& session, const std::string& device_id) {
    return session.device_a == device_id || session.device_b == device_id;
}

}  // namespace

json BuildRelayInfoFromEnvironment() {
    const char* relay_host = GetEnvValue(kRelayPublicHostEnv);
    const char* turn_host = GetEnvValue(kTurnPublicHostEnv);
    const char* relay_port = GetEnvValue(kRelayPublicPortEnv);

    return json{
        {"host", relay_host != nullptr
            ? relay_host
            : (turn_host != nullptr ? turn_host : kDefaultRelayPublicHost)},
        {"port", relay_port != nullptr
            ? GetEnvPortOrDefault(kRelayPublicPortEnv, kDefaultRelayPublicPort)
            : GetEnvPortOrDefault(kTurnPortEnv, kDefaultRelayPublicPort)}
    };
}

MessageRouter::MessageRouter(std::shared_ptr<ConnectionManager> manager)
    : manager_(std::move(manager)),
      register_handler_(manager_),
      connect_handler_(manager_),
      service_handler_(manager_)
{
}

asio::awaitable<std::optional<json>> MessageRouter::handle_message(
    const std::string& device_id,
    const Message& message
) {
    try {
        if (message.type != MessageType::REGISTER && message.type != MessageType::PING) {
            if (auto auth_error = RequireRegisteredDevice(manager_, device_id, message.request_id)) {
                co_return auth_error;
            }
        }
        switch (message.type) {
            case MessageType::REGISTER:
                co_return co_await register_handler_.handle_register(device_id, message);
            case MessageType::UNREGISTER:
                co_return co_await register_handler_.handle_unregister(device_id, message);
            case MessageType::CONNECT:
                co_return co_await connect_handler_.handle_connect(device_id, message);
            case MessageType::OFFER:
                co_return co_await connect_handler_.handle_offer(device_id, message);
            case MessageType::ANSWER:
                co_return co_await connect_handler_.handle_answer(device_id, message);
            case MessageType::ICE_CANDIDATE:
                co_return co_await connect_handler_.handle_ice_candidate(device_id, message);
            case MessageType::HEARTBEAT:
                co_return co_await handle_heartbeat(device_id, message);
            case MessageType::PING:
                co_return co_await handle_ping(device_id, message);
            case MessageType::QUERY_DEVICE:
                co_return co_await register_handler_.handle_query_device(device_id, message);
            case MessageType::RELAY_REQUEST:
                co_return co_await handle_relay_request(device_id, message);
            case MessageType::SERVICE_PUBLISH:
                co_return co_await service_handler_.handle_service_publish(device_id, message);
            case MessageType::SERVICE_UNPUBLISH:
                co_return co_await service_handler_.handle_service_unpublish(device_id, message);
            case MessageType::SERVICE_QUERY:
                co_return co_await service_handler_.handle_service_query(device_id, message);
            case MessageType::SERVICE_CONNECT:
                co_return co_await service_handler_.handle_service_connect(device_id, message);
            default:
                ErrorResponse error{
                    ErrorCode::INVALID_REQUEST,
                    "Unknown message type",
                    message.request_id
                };
                co_return error.to_json();
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Error handling message: {}", e.what());
        ErrorResponse error{
            ErrorCode::INTERNAL_ERROR,
            "Failed to process message",
            message.request_id
        };
        co_return error.to_json();
    }
}

asio::awaitable<json> MessageRouter::handle_heartbeat(
    const std::string& device_id,
    const Message& message
) {
    co_await manager_->update_heartbeat(device_id);

    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    co_return json{
        {"type", "heartbeat_ack"},
        {"data", {{"device_id", device_id}}},
        {"timestamp", now}
    };
}

asio::awaitable<json> MessageRouter::handle_ping(
    const std::string& device_id,
    const Message& message
) {
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    co_return json{
        {"type", "pong"},
        {"data", {{"device_id", device_id}}},
        {"timestamp", now}
    };
}

asio::awaitable<json> MessageRouter::handle_relay_request(
    const std::string& device_id,
    const Message& message
) {
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    // Validate required fields
    if (!message.data.contains("session_id")) {
        ErrorResponse error{
            ErrorCode::INVALID_REQUEST,
            "Missing session_id",
            message.request_id
        };
        co_return error.to_json();
    }

    std::string session_id = message.data["session_id"];

    // Get session
    auto session_opt = manager_->get_session(session_id);
    if (!session_opt) {
        ErrorResponse error{
            ErrorCode::INVALID_REQUEST,
            "Invalid session_id",
            message.request_id
        };
        co_return error.to_json();
    }

    auto session = *session_opt;
    if (!IsSessionMember(session, device_id)) {
        ErrorResponse error{
            ErrorCode::UNAUTHORIZED,
            "Caller is not a member of the requested session",
            message.request_id
        };
        co_return error.to_json();
    }

    json relay_info = BuildRelayInfoFromEnvironment();

    // Notify both devices
    json relay_msg = {
        {"type", "relay_response"},
        {"data", {
            {"session_id", session_id},
            {"use_relay", true},
            {"relay_info", relay_info}
        }},
        {"timestamp", now}
    };

    const bool sent_to_a = co_await manager_->send_message(session.device_a, relay_msg);
    const bool sent_to_b = co_await manager_->send_message(session.device_b, relay_msg);
    if (!sent_to_a || !sent_to_b) {
        ErrorResponse error{
            ErrorCode::CONNECTION_FAILED,
            "Failed to deliver relay response to all session members",
            message.request_id
        };
        co_return error.to_json();
    }

    // Mark session to use relay only after both peers were notified.
    manager_->set_relay_mode(session_id);

    co_return json{
        {"type", "relay_response"},
        {"data", {
            {"session_id", session_id},
            {"status", "requested"},
            {"relay_info", relay_info}
        }},
        {"timestamp", now},
        {"request_id", message.request_id.value_or("")}
    };
}

} // namespace signaling
