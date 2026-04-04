#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/http.hpp>
#include <functional>
#include <memory>
#include <string>

namespace p2p {
namespace relay {
class RateLimiter;
}
}

namespace p2p::utils {

namespace asio = boost::asio;
namespace ssl = boost::asio::ssl;
namespace http = boost::beast::http;
using tcp = asio::ip::tcp;

struct AdminHttpResponse {
    http::status status = http::status::ok;
    std::string content_type = "application/json";
    std::string body;
};

using AdminRequestHandler = std::function<AdminHttpResponse(const std::string& target)>;

class AdminHttpServer : public std::enable_shared_from_this<AdminHttpServer> {
public:
    AdminHttpServer(asio::io_context& io_context,
                    std::string host,
                    uint16_t port,
                    AdminRequestHandler handler,
                    ssl::context* ssl_ctx = nullptr,
                    p2p::relay::RateLimiter* rate_limiter = nullptr);

    void Start();
    void Stop();
    bool IsRunning() const;

private:
    void AcceptLoop();

    tcp::acceptor acceptor_;
    std::string host_;
    uint16_t port_;
    AdminRequestHandler handler_;
    ssl::context* ssl_ctx_;
    p2p::relay::RateLimiter* rate_limiter_;
    bool running_ = false;
};

}  // namespace p2p::utils
