#pragma once

#include "models.hpp"
#include "connection_manager.hpp"
#include <boost/asio/awaitable.hpp>
#include <memory>
#include <optional>

namespace signaling {

class ServiceHandler {
public:
    explicit ServiceHandler(std::shared_ptr<ConnectionManager> manager);

    // Service discovery
    asio::awaitable<json> handle_service_publish(
        const std::string& device_id,
        const Message& message
    );

    asio::awaitable<json> handle_service_unpublish(
        const std::string& device_id,
        const Message& message
    );

    asio::awaitable<json> handle_service_query(
        const std::string& device_id,
        const Message& message
    );

    asio::awaitable<std::optional<json>> handle_service_connect(
        const std::string& device_id,
        const Message& message
    );

private:
    std::shared_ptr<ConnectionManager> manager_;
};

} // namespace signaling
