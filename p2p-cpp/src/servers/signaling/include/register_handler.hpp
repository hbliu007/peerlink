#pragma once

#include "models.hpp"
#include "connection_manager.hpp"
#include <boost/asio/awaitable.hpp>
#include <memory>

namespace signaling {

class RegisterHandler {
public:
    explicit RegisterHandler(std::shared_ptr<ConnectionManager> manager);

    // Device lifecycle management
    asio::awaitable<json> handle_register(
        const std::string& device_id,
        const Message& message
    );

    asio::awaitable<json> handle_unregister(
        const std::string& device_id,
        const Message& message
    );

    asio::awaitable<json> handle_query_device(
        const std::string& device_id,
        const Message& message
    );

private:
    std::shared_ptr<ConnectionManager> manager_;
};

} // namespace signaling
