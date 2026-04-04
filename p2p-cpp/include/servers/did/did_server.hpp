#pragma once

#include <boost/asio.hpp>
#include "p2p/utils/admin_http_server.hpp"
#include "p2p/servers/relay/rate_limiter.hpp"
#include <memory>
#include <string>

namespace p2p {
namespace did {

namespace asio = boost::asio;

struct DidServerConfig {
    std::string host = "0.0.0.0";
    uint16_t port = 8081;
    std::string redis_host = "127.0.0.1";
    uint16_t redis_port = 6379;
    std::string jwt_secret;  // Must be set via environment variable JWT_SECRET
    int max_connections = 1000;

    // Rate limiting configuration
    uint32_t rate_limit_rps = 10;           // Requests per second
    uint32_t rate_limit_burst = 20;         // Burst size
    uint32_t rate_limit_ban_threshold = 5;  // Violations before ban
    uint32_t rate_limit_ban_duration = 300; // Ban duration in seconds
};

class DidServer {
public:
    // Standalone mode: owns its own io_context
    explicit DidServer(const DidServerConfig& config);

    // Shared mode: uses external io_context (for gateway)
    DidServer(const DidServerConfig& config, asio::io_context& external_ioc);

    ~DidServer();

    // Blocking run (standalone mode only)
    void Run();

    // Non-blocking start: registers handlers on io_context without blocking
    void Start();

    void Stop();
    asio::io_context& GetIoContext() { return io_context_; }
    bool IsReady() const { return ready_; }

private:
    DidServerConfig config_;
    asio::io_context owned_io_context_;
    asio::io_context& io_context_;
    std::shared_ptr<p2p::utils::AdminHttpServer> http_server_;
    std::unique_ptr<p2p::relay::RateLimiter> rate_limiter_;
    bool running_;
    bool ready_ = false;
    bool owns_io_context_;
};

using DIDServer = DidServer;
using DIDServerConfig = DidServerConfig;

} // namespace did
} // namespace p2p
