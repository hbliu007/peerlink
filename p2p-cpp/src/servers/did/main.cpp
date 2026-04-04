#include "servers/did/did_server.hpp"
#include "p2p/utils/admin_http_server.hpp"
#include "p2p/utils/service_config.hpp"
#include "p2p/utils/logger.hpp"
#include "p2p/security/tls_context.hpp"
#include <csignal>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace ssl = boost::asio::ssl;

std::unique_ptr<p2p::did::DidServer> g_server;
std::shared_ptr<p2p::utils::AdminHttpServer> g_admin_server;

void signalHandler(int /*signal*/) {
    if (g_server) {
        LOG_INFO("Shutting down DID server...");
        if (g_admin_server) {
            g_admin_server->Stop();
        }
        g_server->Stop();
    }
}

int main(int /*argc*/, char* /*argv*/[]) {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    p2p::utils::Logger::Init("peerlink-did.log");

    p2p::utils::DidServiceConfig config = p2p::utils::LoadDidServiceConfig();
    if (config.jwt_secret.empty() || config.jwt_secret == "change-me-in-production") {
        LOG_CRITICAL("JWT_SECRET must be set to a non-default secret.");
        LOG_CRITICAL("Set it before starting: export JWT_SECRET=<your-secret>");
        return 1;
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

    try {
        p2p::did::DidServerConfig did_config;
        did_config.host = config.host;
        did_config.port = config.port;
        did_config.redis_host = config.redis_host;
        did_config.redis_port = config.redis_port;
        did_config.jwt_secret = config.jwt_secret;
        did_config.max_connections = config.max_connections;

        g_server = std::make_unique<p2p::did::DidServer>(did_config);
        LOG_INFO("Starting DID Server...");
        if (!config.config_path.empty()) {
            LOG_INFO("Config: {}", config.config_path);
        }
        if (config.admin.enabled && config.admin.port != 0) {
            auto& io_context = g_server->GetIoContext();
            g_admin_server = std::make_shared<p2p::utils::AdminHttpServer>(
                io_context,
                config.admin.host,
                config.admin.port,
                [&config](const std::string& target) {
                    const bool ready = g_server && g_server->IsReady();
                    if (target == "/healthz") {
                        return p2p::utils::AdminHttpResponse{
                            boost::beast::http::status::ok,
                            "text/plain",
                            "ok\n"
                        };
                    }
                    if (target == "/readyz") {
                        return p2p::utils::AdminHttpResponse{
                            ready ? boost::beast::http::status::ok
                                  : boost::beast::http::status::service_unavailable,
                            "text/plain",
                            ready ? "ok\n" : "did api not ready\n"
                        };
                    }
                    if (target == "/status") {
                        json body = {
                            {"service", "peerlink-did"},
                            {"ready", ready},
                            {"config", {
                                {"host", config.host},
                                {"port", config.port},
                                {"redis_host", config.redis_host},
                                {"redis_port", config.redis_port},
                                {"max_connections", config.max_connections}
                            }}
                        };
                        return p2p::utils::AdminHttpResponse{
                            boost::beast::http::status::ok,
                            "application/json",
                            body.dump(2)
                        };
                    }
                    return p2p::utils::AdminHttpResponse{
                        boost::beast::http::status::not_found,
                        "application/json",
                        json{{"error", "not_found"}, {"target", target}}.dump()
                    };
                },
                ssl_ctx_ptr);  // 传递 SSL context
            g_admin_server->Start();

            if (ssl_ctx_ptr) {
                LOG_INFO("Admin: https://{}:{}", config.admin.host, config.admin.port);
            } else {
                LOG_INFO("Admin: http://{}:{}", config.admin.host, config.admin.port);
            }
        }
        g_server->Run();
    } catch (const std::exception& e) {
        LOG_CRITICAL("Error: {}", e.what());
        return 1;
    }

    return 0;
}
