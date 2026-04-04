#include "websocket_session.hpp"
#include "connection_manager.hpp"
#include "message_handler.hpp"
#include "p2p/utils/logger.hpp"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <openssl/err.h>

namespace asio = boost::asio;

namespace signaling {

WebSocketSession::WebSocketSession(
    tcp::socket socket,
    std::shared_ptr<ConnectionManager> manager,
    ssl::context* ssl_ctx
)
    : manager_(std::move(manager))
    , is_ssl_(ssl_ctx != nullptr)
{
    // 获取客户端 IP（在移动 socket 前）
    try {
        client_ip_ = socket.remote_endpoint().address().to_string();
    } catch (const std::exception&) {
        client_ip_ = "unknown";
    }

    // 根据是否启用 SSL 创建不同的 stream
    if (is_ssl_) {
        // 构造 SSL stream
        ssl::stream<tcp::socket> ssl_stream(std::move(socket), *ssl_ctx);
        ws_ = std::make_unique<std::variant<
            websocket::stream<tcp::socket>,
            websocket::stream<ssl::stream<tcp::socket>>
        >>(std::in_place_index<1>, std::move(ssl_stream));
        LOG_INFO("Created WSS session from {}", client_ip_);
    } else {
        ws_ = std::make_unique<std::variant<
            websocket::stream<tcp::socket>,
            websocket::stream<ssl::stream<tcp::socket>>
        >>(std::in_place_index<0>, std::move(socket));
        LOG_INFO("Created WS session from {}", client_ip_);
    }
}

WebSocketSession::~WebSocketSession() {
    // Cleanup handled by ConnectionManager
}

asio::awaitable<void> WebSocketSession::run(std::string device_id) {
    device_id_ = std::move(device_id);

    try {
        // SSL 握手（如果启用）
        if (is_ssl_) {
            auto& wss = std::get<websocket::stream<ssl::stream<tcp::socket>>>(*ws_);

            // SSL 握手
            LOG_DEBUG("Starting SSL handshake for {}", device_id_);
            co_await wss.next_layer().async_handshake(
                ssl::stream_base::server,
                asio::use_awaitable
            );
            LOG_DEBUG("SSL handshake completed for {}", device_id_);

            // WebSocket 握手
            co_await wss.async_accept(asio::use_awaitable);
            LOG_INFO("WSS connection established for {}", device_id_);
        } else {
            auto& ws = std::get<websocket::stream<tcp::socket>>(*ws_);

            // WebSocket 握手（明文）
            co_await ws.async_accept(asio::use_awaitable);
            LOG_INFO("WS connection established for {}", device_id_);
        }

        // Send registration confirmation
        json registered_msg = {
            {"type", "registered"},
            {"data", {
                {"device_id", device_id_},
                {"server_time", std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count()}
            }},
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()}
        };

        co_await send(registered_msg);

        // Start read loop
        co_await read_loop();

    } catch (const boost::system::system_error& e) {
        if (e.code() == ssl::error::stream_truncated) {
            LOG_WARN("SSL connection closed by peer: {}", device_id_);
        } else if (e.code().category() == boost::asio::error::get_ssl_category()) {
            LOG_ERROR("SSL error for {}: {} (OpenSSL: {})",
                device_id_, e.what(),
                ERR_error_string(e.code().value(), nullptr));
        } else {
            LOG_ERROR("WebSocket error for {}: {}", device_id_, e.what());
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Unexpected error for {}: {}", device_id_, e.what());
    }

    // Cleanup
    co_await manager_->disconnect_session(device_id_, shared_from_this());
}

asio::awaitable<void> WebSocketSession::send(json message) {
    std::string msg_str = message.dump();

    if (is_ssl_) {
        auto& wss = std::get<websocket::stream<ssl::stream<tcp::socket>>>(*ws_);
        co_await wss.async_write(
            asio::buffer(msg_str),
            asio::use_awaitable
        );
    } else {
        auto& ws = std::get<websocket::stream<tcp::socket>>(*ws_);
        co_await ws.async_write(
            asio::buffer(msg_str),
            asio::use_awaitable
        );
    }
}

asio::awaitable<void> WebSocketSession::close() {
    try {
        if (is_ssl_) {
            auto& wss = std::get<websocket::stream<ssl::stream<tcp::socket>>>(*ws_);
            co_await wss.async_close(
                websocket::close_code::normal,
                asio::use_awaitable
            );
        } else {
            auto& ws = std::get<websocket::stream<tcp::socket>>(*ws_);
            co_await ws.async_close(
                websocket::close_code::normal,
                asio::use_awaitable
            );
        }
    } catch (const std::exception&) {
        // Ignore errors during close
    }
}

asio::awaitable<void> WebSocketSession::read_loop() {
    MessageHandler handler(manager_);

    while (true) {
        try {
            // Read message
            buffer_.clear();

            if (is_ssl_) {
                auto& wss = std::get<websocket::stream<ssl::stream<tcp::socket>>>(*ws_);
                co_await wss.async_read(buffer_, asio::use_awaitable);
            } else {
                auto& ws = std::get<websocket::stream<tcp::socket>>(*ws_);
                co_await ws.async_read(buffer_, asio::use_awaitable);
            }

            // Check rate limit
            if (!manager_->CheckRateLimit(client_ip_)) {
                ErrorResponse error{
                    ErrorCode::RATE_LIMIT_EXCEEDED,
                    "Too many messages, please slow down",
                    ""
                };
                co_await send(error.to_json());
                continue;
            }

            // Parse JSON
            std::string msg_str = beast::buffers_to_string(buffer_.data());
            json j = json::parse(msg_str);

            // Parse message
            Message message = Message::from_json(j);
            message.source_device_id = device_id_;

            // Handle message
            auto response = co_await handler.handle_message(device_id_, message);

            // Send response if any
            if (response) {
                if ((*response).value("type", "") == "registered" &&
                    (*response).contains("data") &&
                    (*response)["data"].contains("device_id")) {
                    device_id_ = (*response)["data"]["device_id"].get<std::string>();
                }
                co_await send(*response);
            }

        } catch (const beast::system_error& e) {
            if (e.code() == websocket::error::closed) {
                // Connection closed normally
                break;
            }
            throw;
        } catch (const json::exception& e) {
            LOG_ERROR("JSON parse error from {}: {}", device_id_, e.what());

            // Send error response
            ErrorResponse error{
                ErrorCode::INVALID_REQUEST,
                std::string("Invalid JSON: ") + e.what(),
                "" // request_id
            };
            // Cannot use co_await in catch block
            auto executor = is_ssl_ ?
                std::get<websocket::stream<ssl::stream<tcp::socket>>>(*ws_).get_executor() :
                std::get<websocket::stream<tcp::socket>>(*ws_).get_executor();

            asio::co_spawn(
                executor,
                send(error.to_json()),
                asio::detached
            );
        }
    }
}

asio::awaitable<void> WebSocketSession::handle_message(const json& message) {
    // This is now handled by MessageHandler in read_loop
    co_return;
}

} // namespace signaling