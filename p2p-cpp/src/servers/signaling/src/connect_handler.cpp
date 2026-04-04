#include "connect_handler.hpp"
#include "p2p/utils/logger.hpp"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace asio = boost::asio;

namespace signaling {
namespace {

bool IsSessionMember(const ConnectionSession& session, const std::string& device_id) {
    return session.device_a == device_id || session.device_b == device_id;
}

}  // namespace

ConnectHandler::ConnectHandler(std::shared_ptr<ConnectionManager> manager)
    : manager_(std::move(manager))
{
}

asio::awaitable<std::optional<json>> ConnectHandler::handle_connect(
    const std::string& device_id,
    const Message& message
) {
    // Get target device ID
    if (!message.data.contains("target_device_id")) {
        ErrorResponse error{
            ErrorCode::INVALID_REQUEST,
            "Missing target_device_id",
            message.request_id
        };
        co_return error.to_json();
    }

    std::string target_device_id = message.data["target_device_id"];

    // Check if target device is connected
    if (!manager_->is_connected(target_device_id)) {
        ErrorResponse error{
            ErrorCode::DEVICE_NOT_FOUND,
            "Target device " + target_device_id + " not found",
            message.request_id
        };
        co_return error.to_json();
    }

    // Check if session already exists
    auto existing = manager_->get_session_by_devices(device_id, target_device_id);
    if (existing && existing->status != ConnectionStatus::DISCONNECTED) {
        auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

        co_return json{
            {"type", "connect_response"},
            {"data", {
                {"session_id", existing->session_id},
                {"status", to_string(existing->status)},
                {"existing", true}
            }},
            {"timestamp", now},
            {"request_id", message.request_id.value_or("")}
        };
    }

    // Create new session
    auto session = manager_->create_session(device_id, target_device_id);
    manager_->add_pending_request(session.session_id, device_id);

    // Forward connection request to target device
    boost::uuids::uuid request_uuid = boost::uuids::random_generator()();
    std::string request_id = boost::uuids::to_string(request_uuid);

    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    json connect_request = {
        {"type", "connect_request"},
        {"data", {
            {"source_device_id", device_id},
            {"session_id", session.session_id},
            {"capabilities", message.data.value("capabilities", json::array())}
        }},
        {"timestamp", now},
        {"request_id", request_id}
    };

    bool sent = co_await manager_->send_message(target_device_id, connect_request);

    if (!sent) {
        manager_->remove_pending_request(session.session_id);
        manager_->remove_session(session.session_id);
        ErrorResponse error{
            ErrorCode::CONNECTION_FAILED,
            "Failed to reach target device",
            message.request_id
        };
        co_return error.to_json();
    }

    co_return json{
        {"type", "connect_response"},
        {"data", {
            {"session_id", session.session_id},
            {"status", "connecting"}
        }},
        {"timestamp", now},
        {"request_id", message.request_id.value_or("")}
    };
}

asio::awaitable<json> ConnectHandler::handle_offer(
    const std::string& device_id,
    const Message& message
) {
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    // Validate required fields
    if (!message.data.contains("session_id") || !message.data.contains("offer")) {
        ErrorResponse error{
            ErrorCode::INVALID_REQUEST,
            "Missing session_id or offer",
            message.request_id
        };
        co_return error.to_json();
    }

    std::string session_id = message.data["session_id"];
    std::string offer = message.data["offer"];

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

    // Store offer
    manager_->set_session_offer(session_id, offer);

    // Determine target device
    std::string target_device_id = (device_id == session.device_a)
        ? session.device_b
        : session.device_a;

    // Forward offer
    json offer_msg = {
        {"type", "offer"},
        {"data", {
            {"session_id", session_id},
            {"offer", offer},
            {"source_device_id", device_id}
        }},
        {"timestamp", now}
    };

    co_await manager_->send_message(target_device_id, offer_msg);

    co_return json{
        {"type", "offer"},
        {"data", {
            {"session_id", session_id},
            {"status", "forwarded"}
        }},
        {"timestamp", now},
        {"request_id", message.request_id.value_or("")}
    };
}

asio::awaitable<json> ConnectHandler::handle_answer(
    const std::string& device_id,
    const Message& message
) {
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    // Validate required fields
    if (!message.data.contains("session_id") || !message.data.contains("answer")) {
        ErrorResponse error{
            ErrorCode::INVALID_REQUEST,
            "Missing session_id or answer",
            message.request_id
        };
        co_return error.to_json();
    }

    std::string session_id = message.data["session_id"];
    std::string answer = message.data["answer"];

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

    // Store answer
    manager_->set_session_answer(session_id, answer);

    // Determine target device
    std::string target_device_id = (device_id == session.device_b)
        ? session.device_a
        : session.device_b;

    // Forward answer
    json answer_msg = {
        {"type", "answer"},
        {"data", {
            {"session_id", session_id},
            {"answer", answer},
            {"source_device_id", device_id}
        }},
        {"timestamp", now}
    };

    co_await manager_->send_message(target_device_id, answer_msg);

    // Update session status
    manager_->update_session_status(session_id, ConnectionStatus::CONNECTED);

    co_return json{
        {"type", "answer"},
        {"data", {
            {"session_id", session_id},
            {"status", "forwarded"}
        }},
        {"timestamp", now},
        {"request_id", message.request_id.value_or("")}
    };
}

asio::awaitable<json> ConnectHandler::handle_ice_candidate(
    const std::string& device_id,
    const Message& message
) {
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    // Validate required fields
    if (!message.data.contains("session_id") || !message.data.contains("candidate")) {
        ErrorResponse error{
            ErrorCode::INVALID_REQUEST,
            "Missing session_id or candidate",
            message.request_id
        };
        co_return error.to_json();
    }

    std::string session_id = message.data["session_id"];
    json candidate = message.data["candidate"];

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

    // Store candidate
    json candidate_data = {
        {"candidate", candidate},
        {"sdpMid", message.data.value("sdpMid", "")},
        {"sdpMLineIndex", message.data.value("sdpMLineIndex", 0)}
    };
    manager_->add_ice_candidate(session_id, device_id, candidate_data);

    // Determine target device
    std::string target_device_id = (device_id == session.device_a)
        ? session.device_b
        : session.device_a;

    // Forward ICE candidate
    json ice_msg = {
        {"type", "ice_candidate"},
        {"data", {
            {"session_id", session_id},
            {"candidate", candidate},
            {"sdpMid", message.data.value("sdpMid", "")},
            {"sdpMLineIndex", message.data.value("sdpMLineIndex", 0)},
            {"source_device_id", device_id}
        }},
        {"timestamp", now}
    };

    co_await manager_->send_message(target_device_id, ice_msg);

    co_return json{
        {"type", "ice_candidate"},
        {"data", {
            {"session_id", session_id},
            {"status", "forwarded"}
        }},
        {"timestamp", now},
        {"request_id", message.request_id.value_or("")}
    };
}

} // namespace signaling
