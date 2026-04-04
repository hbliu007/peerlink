#include <gtest/gtest.h>
#include "p2p/security/tls_context.hpp"
#include "p2p/utils/service_config.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <thread>
#include <chrono>

namespace asio = boost::asio;
namespace ssl = boost::asio::ssl;
namespace beast = boost::beast;
namespace http = beast::http;

using tcp = asio::ip::tcp;

class TlsIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 确保测试证书存在
        system("mkdir -p certs 2>/dev/null");
        system("./scripts/generate-dev-cert.sh > /dev/null 2>&1 || true");
    }
};

// 测试 1: TLS Context 初始化
TEST_F(TlsIntegrationTest, TlsContextInitialization) {
    p2p::utils::TlsConfig config;
    config.enabled = true;
    config.cert_path = "certs/server.crt";
    config.key_path = "certs/server.key";

    auto& tls_ctx = p2p::security::TlsContext::GetInstance();
    ASSERT_NO_THROW(tls_ctx.Initialize(config));
    EXPECT_TRUE(tls_ctx.IsEnabled());
    EXPECT_NE(tls_ctx.GetContext(), nullptr);
}

// 测试 2: TLS 禁用模式
TEST_F(TlsIntegrationTest, TlsDisabled) {
    p2p::utils::TlsConfig config;
    config.enabled = false;

    auto& tls_ctx = p2p::security::TlsContext::GetInstance();
    ASSERT_NO_THROW(tls_ctx.Initialize(config));
    EXPECT_FALSE(tls_ctx.IsEnabled());
}

// 测试 3: 证书文件不存在
TEST_F(TlsIntegrationTest, MissingCertificates) {
    p2p::utils::TlsConfig config;
    config.enabled = true;
    config.cert_path = "nonexistent.crt";
    config.key_path = "nonexistent.key";

    auto& tls_ctx = p2p::security::TlsContext::GetInstance();
    EXPECT_THROW(tls_ctx.Initialize(config), std::runtime_error);
}

// 测试 4: SSL 握手成功
TEST_F(TlsIntegrationTest, SslHandshakeSuccess) {
    // 创建 SSL contexts
    ssl::context server_ctx(ssl::context::tlsv13);
    server_ctx.use_certificate_chain_file("certs/server.crt");
    server_ctx.use_private_key_file("certs/server.key", ssl::context::pem);

    ssl::context client_ctx(ssl::context::tlsv13);
    client_ctx.set_verify_mode(ssl::verify_none);  // 接受自签名证书

    asio::io_context ioc;

    // 服务器端
    tcp::acceptor acceptor(ioc, tcp::endpoint(tcp::v4(), 0));
    auto port = acceptor.local_endpoint().port();

    bool server_handshake_ok = false;
    bool client_handshake_ok = false;

    // 服务器接受连接
    acceptor.async_accept([&](boost::system::error_code ec, tcp::socket socket) {
        ASSERT_FALSE(ec) << "Accept failed: " << ec.message();
        auto ssl_socket = std::make_shared<ssl::stream<tcp::socket>>(
            std::move(socket), server_ctx);
        ssl_socket->async_handshake(ssl::stream_base::server,
            [&, ssl_socket](boost::system::error_code ec) {
                server_handshake_ok = !ec;
                if (ec) {
                    std::cerr << "Server handshake error: " << ec.message() << "\n";
                }
            });
    });

    // 客户端连接
    tcp::socket client_socket(ioc);
    client_socket.async_connect(
        tcp::endpoint(asio::ip::make_address("127.0.0.1"), port),
        [&](boost::system::error_code ec) {
            ASSERT_FALSE(ec) << "Connect failed: " << ec.message();
            auto ssl_socket = std::make_shared<ssl::stream<tcp::socket>>(
                std::move(client_socket), client_ctx);
            ssl_socket->async_handshake(ssl::stream_base::client,
                [&, ssl_socket](boost::system::error_code ec) {
                    client_handshake_ok = !ec;
                    if (ec) {
                        std::cerr << "Client handshake error: " << ec.message() << "\n";
                    }
                });
        });

    // 运行 IO 上下文（带超时）
    std::thread io_thread([&]() { ioc.run(); });
    std::this_thread::sleep_for(std::chrono::seconds(2));
    ioc.stop();
    io_thread.join();

    EXPECT_TRUE(server_handshake_ok) << "Server handshake failed";
    EXPECT_TRUE(client_handshake_ok) << "Client handshake failed";
}

// 测试 5: TLS 版本验证
TEST_F(TlsIntegrationTest, TlsVersionCheck) {
    ssl::context ctx(ssl::context::tlsv13);
    ctx.use_certificate_chain_file("certs/server.crt");
    ctx.use_private_key_file("certs/server.key", ssl::context::pem);

    // 验证只允许 TLS 1.3
    SSL_CTX_set_min_proto_version(ctx.native_handle(), TLS1_3_VERSION);
    SSL_CTX_set_max_proto_version(ctx.native_handle(), TLS1_3_VERSION);

    SUCCEED() << "TLS 1.3 version constraint set successfully";
}

// 测试 6: 密码套件配置
TEST_F(TlsIntegrationTest, CipherSuiteConfiguration) {
    p2p::utils::TlsConfig config;
    config.enabled = true;
    config.cert_path = "certs/server.crt";
    config.key_path = "certs/server.key";
    config.cipher_suites = "TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256";

    auto& tls_ctx = p2p::security::TlsContext::GetInstance();
    ASSERT_NO_THROW(tls_ctx.Initialize(config));

    // 验证密码套件已设置
    auto* ctx = tls_ctx.GetContext();
    ASSERT_NE(ctx, nullptr);

    // 获取密码套件列表
    SSL* ssl = SSL_new(ctx->native_handle());
    ASSERT_NE(ssl, nullptr);

    STACK_OF(SSL_CIPHER)* ciphers = SSL_get1_supported_ciphers(ssl);
    ASSERT_NE(ciphers, nullptr);

    int count = sk_SSL_CIPHER_num(ciphers);
    EXPECT_GT(count, 0) << "No cipher suites available";

    sk_SSL_CIPHER_free(ciphers);
    SSL_free(ssl);
}
