#pragma once

#include "models.hpp"
#include "connection_manager.hpp"
#include "register_handler.hpp"
#include "connect_handler.hpp"
#include "service_handler.hpp"
#include <boost/asio/awaitable.hpp>
#include <memory>
#include <optional>

namespace signaling {

json BuildRelayInfoFromEnvironment();

class MessageRouter {
public:
    explicit MessageRouter(std::shared_ptr<ConnectionManager> manager);

    // Main message routing
    asio::awaitable<std::optional<json>> handle_message(
        const std::string& device_id,
        const Message& message
    );

    // General handlers
    asio::awaitable<json> handle_heartbeat(
        const std::string& device_id,
        const Message& message
    );

    asio::awaitable<json> handle_ping(
        const std::string& device_id,
        const Message& message
    );

    asio::awaitable<json> handle_relay_request(
        const std::string& device_id,
        const Message& message
    );

private:
    std::shared_ptr<ConnectionManager> manager_;
    RegisterHandler register_handler_;
    ConnectHandler connect_handler_;
    ServiceHandler service_handler_;
};

} // namespace signaling
