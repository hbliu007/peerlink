#pragma once

#include "models.hpp"
#include "connection_manager.hpp"
#include <boost/asio/awaitable.hpp>
#include <memory>
#include <optional>

namespace signaling {

class ConnectHandler {
public:
    explicit ConnectHandler(std::shared_ptr<ConnectionManager> manager);

    // Connection negotiation
    asio::awaitable<std::optional<json>> handle_connect(
        const std::string& device_id,
        const Message& message
    );

    asio::awaitable<json> handle_offer(
        const std::string& device_id,
        const Message& message
    );

    asio::awaitable<json> handle_answer(
        const std::string& device_id,
        const Message& message
    );

    asio::awaitable<json> handle_ice_candidate(
        const std::string& device_id,
        const Message& message
    );

private:
    std::shared_ptr<ConnectionManager> manager_;
};

} // namespace signaling
