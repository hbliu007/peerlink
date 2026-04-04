#define SPDLOG_NO_FMT_STRING

#include "p2p/utils/admin_http_server.hpp"
#include "p2p/utils/http_security.hpp"
#include "p2p/servers/relay/rate_limiter.hpp"
#include "p2p/utils/logger.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/ssl.hpp>
#include <optional>

namespace p2p::utils {
namespace {

namespace beast = boost::beast;
namespace ssl = boost::asio::ssl;

class AdminHttpSession : public std::enable_shared_from_this<AdminHttpSession> {
public:
    AdminHttpSession(tcp::socket socket, AdminRequestHandler handler,
                     ssl::context* ssl_ctx, p2p::relay::RateLimiter* rate_limiter)
        : socket_(std::move(socket))
        , ssl_ctx_(ssl_ctx)
        , handler_(std::move(handler))
        , rate_limiter_(rate_limiter) {
        // 获取客户端 IP（在移动 socket 前）
        try {
            client_ip_ = socket_.remote_endpoint().address().to_string();
        } catch (...) {
            client_ip_ = "unknown";
        }

        // 如果启用 SSL，创建 SSL stream
        if (ssl_ctx_) {
            ssl_socket_.emplace(std::move(socket_), *ssl_ctx_);
        }
    }

    void Start() {
        if (ssl_ctx_) {
            // HTTPS 模式：先进行 SSL 握手
            auto self = shared_from_this();
            ssl_socket_->async_handshake(ssl::stream_base::server,
                [self](boost::system::error_code ec) {
                    if (!ec) {
                        self->ReadRequest();
                    } else {
                        std::string msg = "SSL handshake failed from " +
                            self->client_ip_ + ": " + ec.message();
                        LOG_ERROR("{}", msg);
                    }
                });
        } else {
            // HTTP 模式：直接读取请求
            ReadRequest();
        }
    }

private:
    void ReadRequest() {
        auto self = shared_from_this();

        if (ssl_ctx_) {
            // HTTPS 读取
            http::async_read(*ssl_socket_, buffer_, request_,
                [self](const beast::error_code& ec, std::size_t) {
                    if (!ec) {
                        self->ProcessRequest();
                    } else {
                        std::string msg = "HTTPS read error from " +
                            self->client_ip_ + ": " + ec.message();
                        LOG_ERROR("{}", msg);
                    }
                });
        } else {
            // HTTP 读取
            http::async_read(socket_, buffer_, request_,
                [self](const beast::error_code& ec, std::size_t) {
                    if (!ec) {
                        self->ProcessRequest();
                    } else {
                        std::string msg = "HTTP read error from " +
                            self->client_ip_ + ": " + ec.message();
                        LOG_ERROR("{}", msg);
                    }
                });
        }
    }

    void ProcessRequest() {
        // 速率限制检查
        if (rate_limiter_ && !rate_limiter_->AllowRequest(client_ip_)) {
            SendRateLimitResponse();
            return;
        }

        // 处理请求
        AdminHttpResponse result = handler_(std::string(request_.target()));
        response_.result(result.status);
        response_.version(request_.version());
        response_.set(http::field::server, "peerlink-admin");
        response_.set(http::field::content_type, result.content_type);
        response_.keep_alive(false);

        // 添加安全响应头
        p2p::utils::AddSecurityHeaders(response_);

        response_.body() = std::move(result.body);
        response_.prepare_payload();

        WriteResponse();
    }

    void WriteResponse() {
        auto self = shared_from_this();

        if (ssl_ctx_) {
            // HTTPS 写入
            http::async_write(*ssl_socket_, response_,
                [self](const beast::error_code& ec, std::size_t) {
                    if (!ec) {
                        // SSL 优雅关闭
                        self->ssl_socket_->async_shutdown(
                            [](boost::system::error_code) {
                                // 忽略 shutdown 错误（客户端可能已关闭）
                            });
                    } else {
                        std::string msg = "HTTPS write error to " +
                            self->client_ip_ + ": " + ec.message();
                        LOG_ERROR("{}", msg);
                    }
                });
        } else {
            // HTTP 写入
            http::async_write(socket_, response_,
                [self](const beast::error_code& ec, std::size_t) {
                    if (!ec) {
                        // TCP 优雅关闭
                        beast::error_code shutdown_ec;
                        self->socket_.shutdown(tcp::socket::shutdown_send, shutdown_ec);
                    } else {
                        std::string msg = "HTTP write error to " +
                            self->client_ip_ + ": " + ec.message();
                        LOG_ERROR("{}", msg);
                    }
                });
        }
    }

    void SendRateLimitResponse() {
        response_.result(http::status::too_many_requests);
        response_.version(request_.version());
        response_.set(http::field::server, "peerlink-admin");
        response_.set(http::field::content_type, "application/json");
        response_.set(http::field::retry_after, "60");
        response_.keep_alive(false);

        // 添加安全响应头
        p2p::utils::AddSecurityHeaders(response_);

        response_.body() = R"({"error":"rate_limit_exceeded","message":"Too many requests"})";
        response_.prepare_payload();

        WriteResponse();
    }

    tcp::socket socket_;
    std::optional<ssl::stream<tcp::socket>> ssl_socket_;
    ssl::context* ssl_ctx_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> request_;
    http::response<http::string_body> response_;
    AdminRequestHandler handler_;
    p2p::relay::RateLimiter* rate_limiter_;
    std::string client_ip_;
};

}  // namespace

AdminHttpServer::AdminHttpServer(asio::io_context& io_context,
                                 std::string host,
                                 uint16_t port,
                                 AdminRequestHandler handler,
                                 ssl::context* ssl_ctx,
                                 p2p::relay::RateLimiter* rate_limiter)
    : acceptor_(io_context)
    , host_(std::move(host))
    , port_(port)
    , handler_(std::move(handler))
    , ssl_ctx_(ssl_ctx)
    , rate_limiter_(rate_limiter) {}

void AdminHttpServer::Start() {
    if (running_) {
        return;
    }

    tcp::endpoint endpoint(asio::ip::make_address(host_), port_);
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(asio::socket_base::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen(asio::socket_base::max_listen_connections);
    running_ = true;

    if (ssl_ctx_) {
        std::string msg = "AdminHttpServer listening on https://" + host_ + ":" + std::to_string(port_);
        LOG_INFO("{}", msg);
    } else {
        std::string msg = "AdminHttpServer listening on http://" + host_ + ":" + std::to_string(port_) + " (INSECURE)";
        LOG_WARN("{}", msg);
    }

    AcceptLoop();
}

void AdminHttpServer::Stop() {
    if (!running_) {
        return;
    }

    running_ = false;
    beast::error_code ec;
    acceptor_.cancel(ec);
    acceptor_.close(ec);
}

bool AdminHttpServer::IsRunning() const {
    return running_;
}

void AdminHttpServer::AcceptLoop() {
    auto self = shared_from_this();
    acceptor_.async_accept(
        [self](const beast::error_code& ec, tcp::socket socket) {
            if (ec) {
                return;
            }
            std::make_shared<AdminHttpSession>(
                std::move(socket), self->handler_, self->ssl_ctx_, self->rate_limiter_
            )->Start();
            if (self->running_) {
                self->AcceptLoop();
            }
        });
}

}  // namespace p2p::utils
