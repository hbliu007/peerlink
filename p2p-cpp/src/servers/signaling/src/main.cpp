#include "connection_manager.hpp"
#include "websocket_session.hpp"
#include "p2p/utils/admin_http_server.hpp"
#include "p2p/utils/service_config.hpp"
#include "p2p/utils/logger.hpp"
#include "p2p/servers/relay/rate_limiter.hpp"
#include "p2p/security/tls_context.hpp"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <cstdlib>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <openssl/err.h>

namespace asio = boost::asio;
namespace ssl = asio::ssl;
using json = nlohmann::json;

namespace signaling {

using boost::asio::ip::tcp;
using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::use_awaitable;

// Listener for accepting connections
class Listener : public std::enable_shared_from_this<Listener> {
public:
    Listener(
        boost::asio::io_context& ioc,
        tcp::endpoint endpoint,
        std::shared_ptr<ConnectionManager> manager,
        ssl::context* ssl_ctx = nullptr
    )
        : ioc_(ioc)
        , acceptor_(ioc)
        , manager_(std::move(manager))
        , ssl_ctx_(ssl_ctx)
        , ready_(false)
    {
        boost::system::error_code ec;

        // Open acceptor
        acceptor_.open(endpoint.protocol(), ec);
        if (ec) {
            LOG_ERROR("Open error: {}", ec.message());
            return;
        }

        // Set SO_REUSEADDR
        acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
        if (ec) {
            LOG_ERROR("Set option error: {}", ec.message());
            return;
        }

        // Bind
        acceptor_.bind(endpoint, ec);
        if (ec) {
            LOG_ERROR("Bind error: {}", ec.message());
            return;
        }

        // Listen
        acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
        if (ec) {
            LOG_ERROR("Listen error: {}", ec.message());
            return;
        }

        if (ssl_ctx_) {
            LOG_INFO("Signaling listener ready (WSS mode) on {}:{}",
                endpoint.address().to_string(), endpoint.port());
        } else {
            LOG_WARN("Signaling listener ready (WS mode - INSECURE) on {}:{}",
                endpoint.address().to_string(), endpoint.port());
        }
        ready_ = true;
    }

    void run() {
        co_spawn(
            ioc_,
            accept_loop(),
            detached
        );
    }

    bool is_ready() const {
        return ready_;
    }

private:
    awaitable<void> accept_loop() {
        while (true) {
            try {
                // Accept connection
                tcp::socket socket = co_await acceptor_.async_accept(use_awaitable);

                LOG_DEBUG("New connection from {}:{}",
                    socket.remote_endpoint().address().to_string(),
                    socket.remote_endpoint().port());

                // Create session（传递 SSL context）
                auto session = std::make_shared<WebSocketSession>(
                    std::move(socket),
                    manager_,
                    ssl_ctx_
                );

                // Generate temporary device ID (will be replaced by proper registration)
                std::string device_id = "temp_" + std::to_string(
                    std::chrono::system_clock::now().time_since_epoch().count()
                );

                // Register device
                co_await manager_->connect(
                    device_id,
                    session,
                    "",  // public_key (will be filled in register message)
                    {}   // capabilities
                );

                // Start session
                co_spawn(
                    ioc_,
                    session->run(device_id),
                    detached
                );

            } catch (const std::exception& e) {
                LOG_ERROR("Accept error: {}", e.what());
            }
        }
    }

    boost::asio::io_context& ioc_;
    tcp::acceptor acceptor_;
    std::shared_ptr<ConnectionManager> manager_;
    ssl::context* ssl_ctx_;
    bool ready_;
};

// Cleanup task for stale connections
awaitable<void> cleanup_task(
    std::shared_ptr<ConnectionManager> manager,
    int interval_seconds,
    int timeout_seconds
) {
    boost::asio::steady_timer timer(co_await boost::asio::this_coro::executor);

    while (true) {
        timer.expires_after(std::chrono::seconds(interval_seconds));
        co_await timer.async_wait(use_awaitable);

        int cleaned = co_await manager->cleanup_stale(timeout_seconds);
        if (cleaned > 0) {
            LOG_INFO("Cleaned up {} stale connections", cleaned);
        }
    }
}

} // namespace signaling

int main(int argc, char* argv[]) {
    try {
        p2p::utils::Logger::Init("peerlink-signaling.log");

        p2p::utils::SignalingServiceConfig config = p2p::utils::LoadSignalingServiceConfig();
        if (argc >= 2) {
            try {
                int port = std::stoi(argv[1]);
                if (port < 0 || port > 65535) {
                    LOG_ERROR("Port must be between 0 and 65535");
                    return 1;
                }
                config.port = static_cast<unsigned short>(port);
            } catch (const std::exception& e) {
                LOG_ERROR("Invalid port number '{}': {}", argv[1], e.what());
                return 1;
            }
        }

        if (config.jwt_secret.empty() || config.jwt_secret == "change-me-in-production") {
            LOG_CRITICAL("JWT_SECRET must be set to a non-default secret.");
            return 1;
        }

        LOG_INFO("Starting Signaling Server...");
        LOG_INFO("Configuration:");
        LOG_INFO("  Host: {}", config.host);
        LOG_INFO("  Port: {}", config.port);
        LOG_INFO("  TLS enabled: {}", config.tls.enabled);
        LOG_INFO("  Heartbeat interval: {}s", config.heartbeat_interval);
        LOG_INFO("  Connection timeout: {}s", config.connection_timeout);
        LOG_INFO("  Relay endpoint: {}:{}", config.turn_public_ip, config.turn_port);
        LOG_INFO("  Rate limit: {} msg/s, burst {}", config.rate_limit.requests_per_second, config.rate_limit.burst_size);

        // 初始化 TLS context（如果启用）
        ssl::context* ssl_ctx_ptr = nullptr;

        if (config.tls.enabled) {
            try {
                // 使用 TlsContext 单例
                auto& tls_ctx = p2p::security::TlsContext::GetInstance();
                tls_ctx.Initialize(config.tls);
                ssl_ctx_ptr = tls_ctx.GetContext();

                LOG_INFO("TLS enabled:");
                LOG_INFO("  Certificate: {}", config.tls.cert_path);
                LOG_INFO("  Private key: {}", config.tls.key_path);
                LOG_INFO("  Cipher suites: {}", config.tls.cipher_suites);
            } catch (const std::exception& e) {
                LOG_ERROR("Failed to initialize TLS: {}", e.what());
                return 1;
            }
        } else {
            LOG_WARN("TLS DISABLED - Using plain WebSocket (INSECURE)");
            LOG_WARN("This is only suitable for development/testing");
        }

        // Create io_context
        boost::asio::io_context ioc{1};  // Single thread

        // Create connection manager with rate limiting
        auto manager = std::make_shared<signaling::ConnectionManager>();

        // Configure rate limiter from service config
        p2p::relay::RateLimitConfig rate_config(
            config.rate_limit.requests_per_second,
            config.rate_limit.burst_size,
            config.rate_limit.ban_threshold,
            config.rate_limit.ban_duration_seconds
        );
        manager->SetRateLimiter(std::make_unique<p2p::relay::RateLimiter>(rate_config));

        // Create listener（传递 SSL context）
        auto listener = std::make_shared<signaling::Listener>(
            ioc,
            signaling::tcp::endpoint{boost::asio::ip::make_address(config.host), config.port},
            manager,
            ssl_ctx_ptr
        );
        if (!listener->is_ready()) {
            LOG_ERROR("Signaling listener failed to start.");
            return 1;
        }

        // Start listener
        listener->run();

        // Start cleanup task
        signaling::co_spawn(
            ioc,
            signaling::cleanup_task(
                manager,
                config.heartbeat_interval,
                config.connection_timeout
            ),
            signaling::detached
        );

        std::shared_ptr<p2p::utils::AdminHttpServer> admin_server;
        if (config.admin.enabled && config.admin.port != 0) {
            admin_server = std::make_shared<p2p::utils::AdminHttpServer>(
                ioc,
                config.admin.host,
                config.admin.port,
                [manager, &config](const std::string& target) {
                    if (target == "/healthz" || target == "/readyz") {
                        return p2p::utils::AdminHttpResponse{
                            boost::beast::http::status::ok,
                            "text/plain",
                            "ok\n"
                        };
                    }
                    if (target == "/status") {
                        json body = {
                            {"service", "peerlink-signaling"},
                            {"config", {
                                {"host", config.host},
                                {"port", config.port},
                                {"heartbeat_interval", config.heartbeat_interval},
                                {"connection_timeout", config.connection_timeout},
                                {"allow_insecure_registration", config.allow_insecure_registration},
                                {"turn_public_ip", config.turn_public_ip},
                                {"turn_port", config.turn_port}
                            }},
                            {"stats", {
                                {"devices", manager->device_count()},
                                {"sessions", manager->session_count()},
                                {"relay_sessions", manager->relay_session_count()},
                                {"services", manager->service_count()}
                            }}
                        };
                        return p2p::utils::AdminHttpResponse{
                            boost::beast::http::status::ok,
                            "application/json",
                            body.dump(2)
                        };
                    }
                    if (target == "/metrics") {
                        std::string body;
                        body += "peerlink_signaling_devices " + std::to_string(manager->device_count()) + "\n";
                        body += "peerlink_signaling_sessions " + std::to_string(manager->session_count()) + "\n";
                        body += "peerlink_signaling_relay_sessions " + std::to_string(manager->relay_session_count()) + "\n";
                        body += "peerlink_signaling_services " + std::to_string(manager->service_count()) + "\n";
                        return p2p::utils::AdminHttpResponse{
                            boost::beast::http::status::ok,
                            "text/plain; version=0.0.4",
                            body
                        };
                    }
                    return p2p::utils::AdminHttpResponse{
                        boost::beast::http::status::not_found,
                        "application/json",
                        json{{"error", "not_found"}, {"target", target}}.dump()
                    };
                });
            admin_server->Start();
            LOG_INFO("  Admin: {}:{}", config.admin.host, config.admin.port);
        }

        // Setup signal handling
        boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto) {
            LOG_INFO("Shutting down...");
            if (admin_server) {
                admin_server->Stop();
            }
            ioc.stop();
        });

        LOG_INFO("Signaling Server started successfully!");
        if (ssl_ctx_ptr) {
            LOG_INFO("Listening on wss://{}:{}", config.host, config.port);
        } else {
            LOG_INFO("Listening on ws://{}:{}", config.host, config.port);
        }

        // Run io_context
        ioc.run();

        LOG_INFO("Signaling Server stopped.");

    } catch (const std::exception& e) {
        LOG_CRITICAL("Fatal error: {}", e.what());
        return 1;
    }

    return 0;
}