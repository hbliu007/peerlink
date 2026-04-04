#include "servers/did/did_server.hpp"

#include <boost/beast/http.hpp>
#include <nlohmann/json.hpp>
#include "p2p/utils/logger.hpp"

namespace p2p {
namespace did {

namespace http = boost::beast::http;
using json = nlohmann::json;

// Standalone mode: owns its own io_context
DidServer::DidServer(const DidServerConfig& config)
    : config_(config)
    , owned_io_context_()
    , io_context_(owned_io_context_)
    , running_(false)
    , ready_(false)
    , owns_io_context_(true) {
    // Initialize rate limiter
    p2p::relay::RateLimitConfig rate_config(
        config_.rate_limit_rps,
        config_.rate_limit_burst,
        config_.rate_limit_ban_threshold,
        config_.rate_limit_ban_duration
    );
    rate_limiter_ = std::make_unique<p2p::relay::RateLimiter>(rate_config);
}

// Shared mode: uses external io_context
DidServer::DidServer(const DidServerConfig& config, asio::io_context& external_ioc)
    : config_(config)
    , owned_io_context_()
    , io_context_(external_ioc)
    , running_(false)
    , ready_(false)
    , owns_io_context_(false) {
    // Initialize rate limiter
    p2p::relay::RateLimitConfig rate_config(
        config_.rate_limit_rps,
        config_.rate_limit_burst,
        config_.rate_limit_ban_threshold,
        config_.rate_limit_ban_duration
    );
    rate_limiter_ = std::make_unique<p2p::relay::RateLimiter>(rate_config);
}

DidServer::~DidServer() {
    Stop();
}

void DidServer::Start() {
    if (running_) {
        return;
    }

    http_server_ = std::make_shared<p2p::utils::AdminHttpServer>(
        io_context_,
        config_.host,
        config_.port,
        [](const std::string& target) {
            if (target == "/v1/token" || target == "/v1/did/register" || target == "/v1/did/resolve") {
                return p2p::utils::AdminHttpResponse{
                    http::status::not_implemented,
                    "application/json",
                    json{
                        {"error", "not_implemented"},
                        {"message", "DID HTTP API is not implemented yet in this build"},
                        {"target", target}
                    }.dump()
                };
            }
            return p2p::utils::AdminHttpResponse{
                http::status::not_found,
                "application/json",
                json{{"error", "not_found"}, {"target", target}}.dump()
            };
        },
        nullptr,  // ssl_ctx
        rate_limiter_.get());
    http_server_->Start();
    running_ = true;
    LOG_INFO("DID Server started on {}:{}", config_.host, config_.port);
    LOG_INFO("Rate limiting enabled: {} req/s, burst {}", config_.rate_limit_rps, config_.rate_limit_burst);
    LOG_INFO("DID API remains unavailable until HTTP handlers are implemented");
}

void DidServer::Run() {
    Start();
    LOG_INFO("DID Server running (blocking)...");
    io_context_.run();
}

void DidServer::Stop() {
    if (running_) {
        running_ = false;
        ready_ = false;
        if (http_server_) {
            http_server_->Stop();
            http_server_.reset();
        }
        if (owns_io_context_) {
            io_context_.stop();
        }
    }
}

} // namespace did
} // namespace p2p
