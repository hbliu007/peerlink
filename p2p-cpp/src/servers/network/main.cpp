/**
 * @file main.cpp
 * @brief PeerLink Network Server — unified STUN + TURN/Relay binary
 *
 * Merges the STUN and Relay servers into a single process with shared
 * lifecycle management and unified signal handling.
 */

#include "stun_server.hpp"
#include "p2p/servers/relay/relay_server.hpp"
#include "p2p/utils/admin_http_server.hpp"
#include "p2p/utils/service_config.hpp"
#include "p2p/security/tls_context.hpp"
#include <boost/asio.hpp>
#include <csignal>
#include "p2p/utils/logger.hpp"
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

namespace asio = boost::asio;
namespace ssl = boost::asio::ssl;
using json = nlohmann::json;

// Global state for signal handler
static std::shared_ptr<p2p::server::StunServer> g_stun_server;
static p2p::relay::RelayServer* g_relay_server = nullptr;
static asio::io_context* g_io_context = nullptr;

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        LOG_INFO("[network] Received shutdown signal");
        if (g_stun_server) {
            g_stun_server->stop();
        }
        if (g_relay_server) {
            g_relay_server->Stop();
        }
        if (g_io_context) {
            g_io_context->stop();
        }
    }
}

// Periodic stats printer using asio timer (replaces blocking sleep loop)
class StatsPrinter : public std::enable_shared_from_this<StatsPrinter> {
public:
    StatsPrinter(asio::io_context& io,
                 p2p::relay::RelayServer& relay,
                 std::chrono::seconds interval)
        : timer_(io), relay_(relay), interval_(interval) {}

    void start() {
        schedule();
    }

private:
    void schedule() {
        timer_.expires_after(interval_);
        auto self = shared_from_this();
        timer_.async_wait([self](const boost::system::error_code& ec) {
            if (ec) return;  // cancelled or error
            self->print();
            self->schedule();
        });
    }

    void print() {
        auto stats = relay_.GetStats();
        LOG_INFO("=== Relay Statistics ===");
        LOG_INFO("Allocations: {}/{} (max: {})",
                  stats.allocations.active_allocations,
                  stats.allocations.total_allocations,
                  stats.allocations.max_allocations);
        LOG_INFO("Port Pool: {} available ({} used)",
                  stats.allocations.port_pool_available,
                  stats.allocations.port_pool_usage);
        LOG_INFO("Relay Sockets: {}", stats.relay_sockets);
        LOG_INFO("Bandwidth: Read={} Write={}",
                  stats.bandwidth.available_read_tokens,
                  stats.bandwidth.available_write_tokens);
        LOG_INFO("Total Bytes: Sent={} Received={}",
                  stats.allocations.total_bytes_sent,
                  stats.allocations.total_bytes_received);
    }

    asio::steady_timer timer_;
    p2p::relay::RelayServer& relay_;
    std::chrono::seconds interval_;
};

int main() {
    try {
        const p2p::utils::NetworkServiceConfig config =
            p2p::utils::LoadNetworkServiceConfig();

        // ── TURN / Relay configuration ──
        p2p::relay::RelayServerConfig relay_cfg;
        relay_cfg.host              = config.turn_host;
        relay_cfg.port              = config.turn_port;
        relay_cfg.public_ip         = config.turn_public_ip;
        relay_cfg.min_port          = config.turn_min_port;
        relay_cfg.max_port          = config.turn_max_port;
        relay_cfg.default_lifetime  = config.turn_lifetime;
        relay_cfg.max_allocations   = config.turn_max_allocations;

        p2p::utils::Logger::Init("peerlink-network.log");

        LOG_INFO("=== PeerLink Network Server ===");
        LOG_INFO("[STUN] {} UDP:{} TCP:{}", config.stun_host, config.stun_udp_port, config.stun_tcp_port);
        LOG_INFO("[TURN] {}:{} public={} ports={}-{}",
                  relay_cfg.host, relay_cfg.port, relay_cfg.public_ip,
                  relay_cfg.min_port, relay_cfg.max_port);
        if (!config.config_path.empty()) {
            LOG_INFO("[CONFIG] {}", config.config_path);
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

        // ── Signal handling ──
        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);

        // ── Create io_context for STUN (and stats timer) ──
        asio::io_context io_context;
        g_io_context = &io_context;

        // ── Start STUN server ──
        g_stun_server = std::make_shared<p2p::server::StunServer>(
            io_context, config.stun_host, config.stun_udp_port, config.stun_tcp_port);
        g_stun_server->start();

        // ── Start Relay server (owns its own io_context + threads) ──
        p2p::relay::RelayServer relay_server(relay_cfg);
        g_relay_server = &relay_server;
        relay_server.Start();

        // ── Start periodic stats printer on the STUN io_context ──
        auto stats_printer = std::make_shared<StatsPrinter>(
            io_context, relay_server, std::chrono::seconds(config.metrics_interval_seconds));
        stats_printer->start();

        std::shared_ptr<p2p::utils::AdminHttpServer> admin_server;
        if (config.admin.enabled && config.admin.port != 0) {
            admin_server = std::make_shared<p2p::utils::AdminHttpServer>(
                io_context,
                config.admin.host,
                config.admin.port,
                [&config, &relay_server](const std::string& target) {
                    if (target == "/healthz" || target == "/readyz") {
                        return p2p::utils::AdminHttpResponse{
                            boost::beast::http::status::ok,
                            "text/plain",
                            "ok\n"
                        };
                    }

                    auto stats = relay_server.GetStats();
                    if (target == "/status") {
                        json body = {
                            {"service", "peerlink-network"},
                            {"config", {
                                {"stun_host", config.stun_host},
                                {"stun_udp_port", config.stun_udp_port},
                                {"stun_tcp_port", config.stun_tcp_port},
                                {"turn_host", config.turn_host},
                                {"turn_port", config.turn_port},
                                {"turn_public_ip", config.turn_public_ip},
                                {"turn_min_port", config.turn_min_port},
                                {"turn_max_port", config.turn_max_port},
                                {"metrics_interval_seconds", config.metrics_interval_seconds}
                            }},
                            {"stats", {
                                {"relay_sockets", stats.relay_sockets},
                                {"active_allocations", stats.allocations.active_allocations},
                                {"total_allocations", stats.allocations.total_allocations},
                                {"port_pool_available", stats.allocations.port_pool_available},
                                {"total_bytes_sent", stats.allocations.total_bytes_sent},
                                {"total_bytes_received", stats.allocations.total_bytes_received},
                                {"available_read_tokens", stats.bandwidth.available_read_tokens},
                                {"available_write_tokens", stats.bandwidth.available_write_tokens}
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
                        body += "peerlink_network_relay_sockets " +
                                std::to_string(stats.relay_sockets) + "\n";
                        body += "peerlink_network_allocations_active " +
                                std::to_string(stats.allocations.active_allocations) + "\n";
                        body += "peerlink_network_allocations_total " +
                                std::to_string(stats.allocations.total_allocations) + "\n";
                        body += "peerlink_network_allocations_bytes_sent_total " +
                                std::to_string(stats.allocations.total_bytes_sent) + "\n";
                        body += "peerlink_network_allocations_bytes_received_total " +
                                std::to_string(stats.allocations.total_bytes_received) + "\n";
                        body += "peerlink_network_bandwidth_read_tokens " +
                                std::to_string(stats.bandwidth.available_read_tokens) + "\n";
                        body += "peerlink_network_bandwidth_write_tokens " +
                                std::to_string(stats.bandwidth.available_write_tokens) + "\n";
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
                LOG_INFO("[ADMIN] https://{}:{}", config.admin.host, config.admin.port);
            } else {
                LOG_INFO("[ADMIN] http://{}:{}", config.admin.host, config.admin.port);
            }
        }

        // ── Run STUN io_context on main thread ──
        io_context.run();

        // ── Cleanup ──
        relay_server.Stop();
        if (admin_server) {
            admin_server->Stop();
        }
        g_stun_server.reset();
        g_relay_server = nullptr;
        g_io_context = nullptr;

        LOG_INFO("[network] Server exited");
        return 0;

    } catch (const std::exception& e) {
        LOG_CRITICAL("[network] Fatal error: {}", e.what());
        return 1;
    }
}
