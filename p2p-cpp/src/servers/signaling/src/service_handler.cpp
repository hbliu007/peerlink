#include "service_handler.hpp"
#include "p2p/utils/logger.hpp"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <algorithm>

namespace asio = boost::asio;

namespace signaling {
namespace {

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

bool ServiceAllowsCaller(const PublishedService& service, const std::string& device_id) {
    if (service.owner_device_id == device_id) {
        return true;
    }
    if (std::find(service.allowed_callers.begin(),
                  service.allowed_callers.end(),
                  "*") != service.allowed_callers.end()) {
        return true;
    }
    return std::find(service.allowed_callers.begin(),
                     service.allowed_callers.end(),
                     device_id) != service.allowed_callers.end();
}

}  // namespace

ServiceHandler::ServiceHandler(std::shared_ptr<ConnectionManager> manager)
    : manager_(std::move(manager))
{
}

asio::awaitable<json> ServiceHandler::handle_service_publish(
    const std::string& device_id,
    const Message& message
) {
    if (!message.data.contains("service_name")) {
        ErrorResponse error{
            ErrorCode::INVALID_REQUEST,
            "Missing service_name",
            message.request_id
        };
        co_return error.to_json();
    }

    auto existing = manager_->get_service(message.data["service_name"].get<std::string>());
    if (existing.has_value() && existing->owner_device_id != device_id) {
        ErrorResponse error{
            ErrorCode::UNAUTHORIZED,
            "service_name is already owned by another device",
            message.request_id
        };
        co_return error.to_json();
    }

    PublishedService service;
    service.name = message.data["service_name"].get<std::string>();
    service.owner_device_id = device_id;
    service.relay_only = message.data.value("relay_only", false);
    std::string parse_error;
    if (!ParseStringList(message.data, "capabilities", service.capabilities, parse_error) ||
        !ParseStringList(message.data, "allowed_callers", service.allowed_callers, parse_error)) {
        ErrorResponse error{
            ErrorCode::INVALID_REQUEST,
            parse_error,
            message.request_id
        };
        co_return error.to_json();
    }
    service.metadata = message.data.value("metadata", json::object());
    service.updated_at = std::chrono::system_clock::now();
    manager_->publish_service(service);

    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        service.updated_at.time_since_epoch()).count();
    co_return json{
        {"type", "service_published"},
        {"data", service.to_json()},
        {"timestamp", now},
        {"request_id", message.request_id.value_or("")}
    };
}

asio::awaitable<json> ServiceHandler::handle_service_unpublish(
    const std::string& device_id,
    const Message& message
) {
    if (!message.data.contains("service_name")) {
        ErrorResponse error{
            ErrorCode::INVALID_REQUEST,
            "Missing service_name",
            message.request_id
        };
        co_return error.to_json();
    }

    const std::string service_name = message.data["service_name"].get<std::string>();
    const bool removed = manager_->unpublish_service(device_id, service_name);
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    co_return json{
        {"type", "service_info"},
        {"data", {
            {"service_name", service_name},
            {"status", removed ? "removed" : "not_found"}
        }},
        {"timestamp", now},
        {"request_id", message.request_id.value_or("")}
    };
}

asio::awaitable<json> ServiceHandler::handle_service_query(
    const std::string& device_id,
    const Message& message
) {
    if (!message.data.contains("service_name")) {
        ErrorResponse error{
            ErrorCode::INVALID_REQUEST,
            "Missing service_name",
            message.request_id
        };
        co_return error.to_json();
    }

    const std::string service_name = message.data["service_name"].get<std::string>();
    auto service = manager_->get_service(service_name);
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    if (!service.has_value() || !ServiceAllowsCaller(*service, device_id)) {
        co_return json{
            {"type", "service_info"},
            {"data", {
                {"service_name", service_name},
                {"online", false}
            }},
            {"timestamp", now},
            {"request_id", message.request_id.value_or("")}
        };
    }

    co_return json{
        {"type", "service_info"},
        {"data", {
            {"service", service->to_json()},
            {"online", manager_->is_connected(service->owner_device_id)}
        }},
        {"timestamp", now},
        {"request_id", message.request_id.value_or("")}
    };
}

asio::awaitable<std::optional<json>> ServiceHandler::handle_service_connect(
    const std::string& device_id,
    const Message& message
) {
    if (!message.data.contains("service_name")) {
        ErrorResponse error{
            ErrorCode::INVALID_REQUEST,
            "Missing service_name",
            message.request_id
        };
        co_return error.to_json();
    }

    const std::string service_name = message.data["service_name"].get<std::string>();
    auto service = manager_->get_service(service_name);
    if (!service.has_value() || !ServiceAllowsCaller(*service, device_id)) {
        ErrorResponse error{
            ErrorCode::DEVICE_NOT_FOUND,
            "Service unavailable",
            message.request_id
        };
        co_return error.to_json();
    }

    auto existing = manager_->get_session_by_devices(device_id, service->owner_device_id);
    if (existing && existing->status != ConnectionStatus::DISCONNECTED) {
        auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        co_return json{
            {"type", "connect_response"},
            {"data", {
                {"session_id", existing->session_id},
                {"status", to_string(existing->status)},
                {"existing", true},
                {"service_name", service_name}
            }},
            {"timestamp", now},
            {"request_id", message.request_id.value_or("")}
        };
    }

    auto session = manager_->create_session(device_id, service->owner_device_id);
    manager_->add_pending_request(session.session_id, device_id);
    if (service->relay_only) {
        manager_->set_relay_mode(session.session_id);
    }

    if (!manager_->is_connected(service->owner_device_id)) {
        manager_->remove_pending_request(session.session_id);
        manager_->remove_session(session.session_id);
        ErrorResponse error{
            ErrorCode::DEVICE_NOT_FOUND,
            "Service unavailable",
            message.request_id
        };
        co_return error.to_json();
    }

    boost::uuids::uuid request_uuid = boost::uuids::random_generator()();
    std::string request_id = boost::uuids::to_string(request_uuid);
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    json connect_request = {
        {"type", "connect_request"},
        {"data", {
            {"source_device_id", device_id},
            {"session_id", session.session_id},
            {"service_name", service->name},
            {"service", service->to_json()},
            {"capabilities", message.data.value("capabilities", json::array())}
        }},
        {"timestamp", now},
        {"request_id", request_id}
    };

    bool sent = co_await manager_->send_message(service->owner_device_id, connect_request);
    if (!sent) {
        manager_->remove_pending_request(session.session_id);
        manager_->remove_session(session.session_id);
        ErrorResponse error{
            ErrorCode::CONNECTION_FAILED,
            "Failed to reach service owner",
            message.request_id
        };
        co_return error.to_json();
    }

    co_return json{
        {"type", "connect_response"},
        {"data", {
            {"session_id", session.session_id},
            {"status", "connecting"},
            {"service_name", service->name},
            {"use_relay", service->relay_only}
        }},
        {"timestamp", now},
        {"request_id", message.request_id.value_or("")}
    };
}

} // namespace signaling
