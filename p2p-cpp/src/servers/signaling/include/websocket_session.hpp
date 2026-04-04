#pragma once

#include "models.hpp"
#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <memory>
#include <string>
#include <variant>

namespace signaling {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace ssl = asio::ssl;
using tcp = asio::ip::tcp;

// Forward declaration
class ConnectionManager;

class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
public:
    explicit WebSocketSession(
        tcp::socket socket,
        std::shared_ptr<ConnectionManager> manager,
        ssl::context* ssl_ctx = nullptr
    );

    ~WebSocketSession();

    // Start the session
    asio::awaitable<void> run(std::string device_id);

    // Send a message
    asio::awaitable<void> send(json message);

    // Close the connection
    asio::awaitable<void> close();

    // Get device ID
    const std::string& device_id() const { return device_id_; }
    void set_device_id(std::string device_id) { device_id_ = std::move(device_id); }

    // Get client IP
    std::string get_client_ip() const { return client_ip_; }

    // Get executor
    auto get_executor() {
        if (is_ssl_) {
            return std::get<websocket::stream<ssl::stream<tcp::socket>>>(*ws_).get_executor();
        } else {
            return std::get<websocket::stream<tcp::socket>>(*ws_).get_executor();
        }
    }

private:
    // Read loop
    asio::awaitable<void> read_loop();

    // Handle incoming message
    asio::awaitable<void> handle_message(const json& message);

    // 使用 unique_ptr + variant 支持两种 stream 类型
    std::unique_ptr<std::variant<
        websocket::stream<tcp::socket>,
        websocket::stream<ssl::stream<tcp::socket>>
    >> ws_;

    std::shared_ptr<ConnectionManager> manager_;
    std::string device_id_;
    std::string client_ip_;
    beast::flat_buffer buffer_;
    bool is_ssl_;
};

} // namespace signaling