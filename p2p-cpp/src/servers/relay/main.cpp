/**
 * @file main.cpp
 * @brief Relay Server Entry Point
 */

#include "p2p/servers/relay/relay_server.hpp"
#include "p2p/utils/logger.hpp"
#include <csignal>
#include <atomic>

using namespace p2p::relay;

std::atomic<bool> g_running{true};
RelayServer* g_server = nullptr;

void signal_handler(int signal) {
    LOG_INFO("Received signal {}, shutting down...", signal);
    g_running = false;
    if (g_server) {
        g_server->Stop();
    }
}

void print_usage(const char* program_name) {
    spdlog::info("Usage: {} [OPTIONS]\n"
        "\nOptions:\n"
        "  --host HOST          Host to bind to (default: 0.0.0.0)\n"
        "  --port PORT          Control port (default: 9001)\n"
        "  --public-ip IP       Public IP address (default: 127.0.0.1)\n"
        "  --min-port PORT      Minimum relay port (default: 50000)\n"
        "  --max-port PORT      Maximum relay port (default: 50100)\n"
        "  --lifetime SECONDS   Default allocation lifetime (default: 600)\n"
        "  --max-allocs NUM     Maximum concurrent allocations (default: 1000)\n"
        "  --help               Show this help message",
        program_name);
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    RelayServerConfig config;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--host" && i + 1 < argc) {
            config.host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            config.port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--public-ip" && i + 1 < argc) {
            config.public_ip = argv[++i];
        } else if (arg == "--min-port" && i + 1 < argc) {
            config.min_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--max-port" && i + 1 < argc) {
            config.max_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--lifetime" && i + 1 < argc) {
            config.default_lifetime = static_cast<uint32_t>(std::stoi(argv[++i]));
        } else if (arg == "--max-allocs" && i + 1 < argc) {
            config.max_allocations = static_cast<size_t>(std::stoi(argv[++i]));
        } else {
            LOG_ERROR("Unknown option: {}", arg);
            print_usage(argv[0]);
            return 1;
        }
    }

    // Setup signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    try {
        p2p::utils::Logger::Init("peerlink-relay.log");

        // Create and start server
        LOG_INFO("Starting Relay/TURN Server...");
        LOG_INFO("Configuration:");
        LOG_INFO("  Host: {}", config.host);
        LOG_INFO("  Port: {}", config.port);
        LOG_INFO("  Public IP: {}", config.public_ip);
        LOG_INFO("  Port Range: {}-{}", config.min_port, config.max_port);
        LOG_INFO("  Default Lifetime: {}s", config.default_lifetime);
        LOG_INFO("  Max Allocations: {}", config.max_allocations);

        RelayServer server(config);
        g_server = &server;

        server.Start();

        // Main loop - print statistics periodically
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::seconds(30));

            if (g_running) {
                auto stats = server.GetStats();
                LOG_INFO("=== Server Statistics ===");
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
        }

        server.Stop();
        g_server = nullptr;

        LOG_INFO("Server shutdown complete");
        return 0;

    } catch (const std::exception& e) {
        LOG_CRITICAL("Fatal error: {}", e.what());
        return 1;
    }
}
