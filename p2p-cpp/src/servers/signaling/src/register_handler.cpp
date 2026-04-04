#include "register_handler.hpp"
#include "servers/did/did_auth.hpp"
#include "p2p/utils/logger.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <limits>

namespace asio = boost::asio;

namespace signaling {
namespace {

constexpr char kJwtSecretEnv[] = "JWT_SECRET";
constexpr char kAllowInsecureRegistrationEnv[] = "SIGNALING_ALLOW_INSECURE_REGISTRATION";

const char* GetEnvValue(const char* name) {
    const char* value = std::getenv(name);
    return (value != nullptr && value[0] != '\0') ? value : nullptr;
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

bool ParseStringList(const json& data,
                     const char* key,
                     std::vector<std::string>& values,
                     std::string& error_message) {
    values.clear();
    if (!data.contains(key)) {
        return true;
    }
    if (!data[key].is_array()) {
        error_message = std::string(key) + " must be an array of strings";
        return false;
    }
    for (const auto& item : data[key]) {
        if (!item.is_string()) {
            error_message = std::string(key) + " must contain only strings";
            return false;
        }
        values.push_back(item.get<std::string>());
    }
    return true;
}

}  // namespace

RegisterHandler::RegisterHandler(std::shared_ptr<ConnectionManager> manager)
    : manager_(std::move(manager))
{
}

asio::awaitable<json> RegisterHandler::handle_register(
    const std::string& device_id,
    const Message& message
) {
    std::string registered_device_id = message.data.value("device_id", device_id);
    std::string public_key = message.data.value("public_key", "");
    std::vector<std::string> capabilities;
    std::string parse_error;
    if (!ParseStringList(message.data, "capabilities", capabilities, parse_error)) {
        ErrorResponse error{
            ErrorCode::INVALID_REQUEST,
            parse_error,
            message.request_id
        };
        co_return error.to_json();
    }
    json metadata = message.data.value("metadata", json::object());

    const std::string auth_token = message.data.value("auth_token", "");
    const char* jwt_secret_value = GetEnvValue(kJwtSecretEnv);
    const bool allow_insecure_registration = GetEnvBool(kAllowInsecureRegistrationEnv, false);
    if (!auth_token.empty()) {
        if (jwt_secret_value == nullptr) {
            ErrorResponse error{
                ErrorCode::UNAUTHORIZED,
                "auth_token was provided but JWT_SECRET is not configured",
                message.request_id
            };
            co_return error.to_json();
        }

        p2p::did::DidAuth auth(jwt_secret_value);
        if (!auth.ValidateToken(auth_token)) {
            ErrorResponse error{
                ErrorCode::UNAUTHORIZED,
                "auth_token is invalid or expired",
                message.request_id
            };
            co_return error.to_json();
        }

        const std::string token_device_id = auth.ExtractDid(auth_token);
        if (token_device_id.empty()) {
            ErrorResponse error{
                ErrorCode::UNAUTHORIZED,
                "auth_token did not contain a valid device identity",
                message.request_id
            };
            co_return error.to_json();
        }

        if (message.data.contains("device_id") && registered_device_id != token_device_id) {
            ErrorResponse error{
                ErrorCode::UNAUTHORIZED,
                "device_id does not match auth_token subject",
                message.request_id
            };
            co_return error.to_json();
        }

        registered_device_id = token_device_id;
        metadata["auth_mode"] = "jwt";
    } else if (!allow_insecure_registration) {
        ErrorResponse error{
            ErrorCode::UNAUTHORIZED,
            "auth_token is required for registration",
            message.request_id
        };
        co_return error.to_json();
    } else {
        metadata["auth_mode"] = "insecure";
    }

    bool registered = co_await manager_->register_device(
        device_id,
        registered_device_id,
        std::move(public_key),
        std::move(capabilities),
        std::move(metadata));
    if (!registered) {
        ErrorResponse error{
            ErrorCode::INTERNAL_ERROR,
            "failed to register device",
            message.request_id
        };
        co_return error.to_json();
    }

    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    co_return json{
        {"type", "registered"},
        {"data", {{"device_id", registered_device_id}}},
        {"timestamp", now}
    };
}

asio::awaitable<json> RegisterHandler::handle_unregister(
    const std::string& device_id,
    const Message& message
) {
    co_await manager_->disconnect(device_id);

    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    co_return json{
        {"type", "unregister"},
        {"data", {{"device_id", device_id}}},
        {"timestamp", now}
    };
}

asio::awaitable<json> RegisterHandler::handle_query_device(
    const std::string& device_id,
    const Message& message
) {
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    // Validate required fields
    if (!message.data.contains("target_device_id")) {
        ErrorResponse error{
            ErrorCode::INVALID_REQUEST,
            "Missing target_device_id",
            message.request_id
        };
        co_return error.to_json();
    }

    std::string target_device_id = message.data["target_device_id"];
    auto device_opt = manager_->get_device(target_device_id);

    if (!device_opt) {
        co_return json{
            {"type", "device_info"},
            {"data", {
                {"device_id", target_device_id},
                {"online", false}
            }},
            {"timestamp", now},
            {"request_id", message.request_id.value_or("")}
        };
    }

    auto device = *device_opt;

    co_return json{
        {"type", "device_info"},
        {"data", {
            {"device_id", target_device_id},
            {"online", true},
            {"capabilities", device.capabilities},
            {"nat_type", to_string(device.nat_type)}
        }},
        {"timestamp", now},
        {"request_id", message.request_id.value_or("")}
    };
}

} // namespace signaling
