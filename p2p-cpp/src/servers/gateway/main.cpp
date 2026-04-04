// PeerLink Gateway - Unified Signaling + DID Server
// Shares a single Boost.Asio io_context for both services

#include "connection_manager.hpp"
#include "websocket_session.hpp"
#include "p2p/utils/admin_http_server.hpp"
#include "p2p/utils/service_config.hpp"
#include "p2p/security/tls_context.hpp"
#include "servers/did/did_server.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <cstdlib>
#include "p2p/utils/logger.hpp"
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

namespace asio = boost::asio;
namespace ssl = boost::asio::ssl;
using json = nlohmann::json;
using boost::asio::ip::tcp;
using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::use_awaitable;

// --- Signaling components (reused from signaling/main.cpp) ---

namespace signaling {

class Listener : public std::enable_shared_from_this<Listener> {
public:
    Listener(
        asio::io_context& ioc,
        tcp::endpoint endpoint,
        std::shared_ptr<ConnectionManager> manager
    )
        : ioc_(ioc)
        , acceptor_(ioc)
        , manager_(std::move(manager))
        , ready_(false)
    {
        boost::system::error_code ec;

        acceptor_.open(endpoint.protocol(), ec);
        if (ec) { LOG_ERROR("[Signaling] Open error: {}", ec.message()); return; }

        acceptor_.set_option(asio::socket_base::reuse_address(true), ec);
        if (ec) { LOG_ERROR("[Signaling] Set option error: {}", ec.message()); return; }

        acceptor_.bind(endpoint, ec);
        if (ec) { LOG_ERROR("[Signaling] Bind error: {}", ec.message()); return; }

        acceptor_.listen(asio::socket_base::max_listen_connections, ec);
        if (ec) { LOG_ERROR("[Signaling] Listen error: {}", ec.message()); return; }

        LOG_INFO("[Signaling] Listening on {}:{}", endpoint.address().to_string(), endpoint.port());
        ready_ = true;
    }

    void run() {
        co_spawn(ioc_, accept_loop(), detached);
    }

    bool is_ready() const {
        return ready_;
    }

private:
    awaitable<void> accept_loop() {
        while (true) {
            try {
                tcp::socket socket = co_await acceptor_.async_accept(use_awaitable);

                LOG_INFO("[Signaling] New connection from {}:{}",
                          socket.remote_endpoint().address().to_string(),
                          socket.remote_endpoint().port());

                auto session = std::make_shared<WebSocketSession>(
                    std::move(socket), manager_
                );

                std::string device_id = "temp_" + std::to_string(
                    std::chrono::system_clock::now().time_since_epoch().count()
                );

                co_await manager_->connect(device_id, session, "", {});

                co_spawn(ioc_, session->run(device_id), detached);

            } catch (const std::exception& e) {
                LOG_ERROR("[Signaling] Accept error: {}", e.what());
            }
        }
    }

    asio::io_context& ioc_;
    tcp::acceptor acceptor_;
    std::shared_ptr<ConnectionManager> manager_;
    bool ready_;
};

awaitable<void> cleanup_task(
    std::shared_ptr<ConnectionManager> manager,
    int interval_seconds,
    int timeout_seconds
) {
    asio::steady_timer timer(co_await boost::asio::this_coro::executor);
    while (true) {
        timer.expires_after(std::chrono::seconds(interval_seconds));
        co_await timer.async_wait(use_awaitable);

        int cleaned = co_await manager->cleanup_stale(timeout_seconds);
        if (cleaned > 0) {
            LOG_INFO("[Signaling] Cleaned up {} stale connections", cleaned);
        }
    }
}

} // namespace signaling

// --- Main ---

int main() {
    try {
        const auto config = p2p::utils::LoadGatewayServiceConfig();
        if (!config.jwt_secret.empty()) {
            setenv("JWT_SECRET", config.jwt_secret.c_str(), 1);
        }
        setenv("SIGNALING_ALLOW_INSECURE_REGISTRATION",
               config.allow_insecure_registration ? "true" : "false", 1);
        if (!config.turn_public_ip.empty()) {
            setenv("TURN_PUBLIC_IP", config.turn_public_ip.c_str(), 1);
        }
        setenv("TURN_PORT", std::to_string(config.turn_port).c_str(), 1);

        if (config.jwt_secret.empty() || config.jwt_secret == "change-me-in-production") {
            LOG_CRITICAL("FATAL: JWT_SECRET must be set to a non-default secret.");
            return 1;
        }

        p2p::utils::Logger::Init("peerlink-gateway.log");

        LOG_INFO("=== PeerLink Gateway ===");
        LOG_INFO("Signaling: {}:{}", config.signaling_host, config.signaling_port);
        LOG_INFO("DID:       {}:{}", config.did_host, config.did_port);
        LOG_INFO("Relay:     {}:{}", config.turn_public_ip, config.turn_port);
        LOG_INFO("Redis:     {}:{}", config.redis_host, config.redis_port);
        if (!config.config_path.empty()) {
            LOG_INFO("Config:    {}", config.config_path);
        }

        // 初始化 TLS context（如果启用）
        ssl::context* ssl_ctx_ptr = nullptr;

        if (config.tls.enabled) {
            try {
                auto& tls_ctx = p2p::security::TlsContext::GetInstance();
                tls_ctx.Initialize(config.tls);
                ssl_ctx_ptr = tls_ctx.GetContext();

                LOG_INFO("TLS enabled:");
                LOG_INFO("  Certificate: {}", config.tls.cert_path);
                LOG_INFO("  Private key: {}", config.tls.key_path);
            } catch (const std::exception& e) {
                LOG_ERROR("Failed to initialize TLS: {}", e.what());
                return 1;
            }
        } else {
            LOG_WARN("TLS DISABLED - Using plain HTTP (INSECURE)");
        }

        // --- Single io_context for both services ---
        asio::io_context ioc{1};

        // --- Start DID Server (non-blocking) ---
        p2p::did::DIDServerConfig did_config;
        did_config.host = config.did_host;
        did_config.port = config.did_port;
        did_config.redis_host = config.redis_host;
        did_config.redis_port = config.redis_port;
        did_config.jwt_secret = config.jwt_secret;

        auto did_server = std::make_unique<p2p::did::DidServer>(did_config, ioc);
        did_server->Start();

        // --- Start Signaling Server ---
        auto manager = std::make_shared<signaling::ConnectionManager>();

        auto listener = std::make_shared<signaling::Listener>(
            ioc,
            tcp::endpoint{asio::ip::make_address(config.signaling_host), config.signaling_port},
            manager
        );
        if (!listener->is_ready()) {
            LOG_ERROR("[Gateway] signaling listener failed to start.");
            return 1;
        }
        listener->run();

        // Start cleanup task
        co_spawn(
            ioc,
            signaling::cleanup_task(manager, config.heartbeat_interval, config.connection_timeout),
            detached
        );

        std::shared_ptr<p2p::utils::AdminHttpServer> admin_server;
        if (config.admin.enabled && config.admin.port != 0) {
            admin_server = std::make_shared<p2p::utils::AdminHttpServer>(
                ioc,
                config.admin.host,
                config.admin.port,
                [manager, &config, &did_server](const std::string& target) {
                    if (target == "/healthz") {
                        return p2p::utils::AdminHttpResponse{
                            boost::beast::http::status::ok,
                            "text/plain",
                            "ok\n"
                        };
                    }
                    if (target == "/readyz") {
                        return p2p::utils::AdminHttpResponse{
                            boost::beast::http::status::ok,
                            "text/plain",
                            "ok\n"
                        };
                    }
                    if (target == "/status") {
                        json body = {
                            {"service", "peerlink-gateway"},
                            {"ready", true},
                            {"did_api_ready", did_server->IsReady()},
                            {"config", {
                                {"signaling_host", config.signaling_host},
                                {"signaling_port", config.signaling_port},
                                {"did_host", config.did_host},
                                {"did_port", config.did_port},
                                {"turn_public_ip", config.turn_public_ip},
                                {"turn_port", config.turn_port},
                                {"redis_host", config.redis_host},
                                {"redis_port", config.redis_port},
                                {"heartbeat_interval", config.heartbeat_interval},
                                {"connection_timeout", config.connection_timeout},
                                {"allow_insecure_registration", config.allow_insecure_registration}
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
                        body += "peerlink_gateway_devices " + std::to_string(manager->device_count()) + "\n";
                        body += "peerlink_gateway_sessions " + std::to_string(manager->session_count()) + "\n";
                        body += "peerlink_gateway_relay_sessions " + std::to_string(manager->relay_session_count()) + "\n";
                        body += "peerlink_gateway_services " + std::to_string(manager->service_count()) + "\n";
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
                },
                ssl_ctx_ptr);  // 传递 SSL context
            admin_server->Start();

            if (ssl_ctx_ptr) {
                LOG_INFO("Admin:     https://{}:{}", config.admin.host, config.admin.port);
            } else {
                LOG_INFO("Admin:     http://{}:{}", config.admin.host, config.admin.port);
            }
        }

        // --- Signal handling ---
        asio::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto) {
            LOG_INFO("[Gateway] Shutting down...");
            if (admin_server) {
                admin_server->Stop();
            }
            did_server->Stop();
            ioc.stop();
        });

        LOG_INFO("[Gateway] Started successfully!");

        // --- Run event loop ---
        ioc.run();

        LOG_INFO("[Gateway] Stopped.");

    } catch (const std::exception& e) {
        LOG_CRITICAL("[Gateway] Fatal error: {}", e.what());
        return 1;
    }

    return 0;
}
