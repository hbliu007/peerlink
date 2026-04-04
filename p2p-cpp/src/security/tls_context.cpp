#include "p2p/security/tls_context.hpp"
#include <filesystem>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <stdexcept>
#include <spdlog/spdlog.h>

namespace p2p::security {

TlsContext& TlsContext::GetInstance() {
    static TlsContext instance;
    return instance;
}

void TlsContext::Initialize(const TlsConfig& config) {
    config_ = config;

    if (!config.enabled) {
        spdlog::info("TLS disabled");
        return;
    }

    spdlog::info("Initializing TLS context...");

    // 验证证书文件存在
    if (!std::filesystem::exists(config.cert_path)) {
        throw std::runtime_error("Certificate file not found: " + config.cert_path);
    }
    if (!std::filesystem::exists(config.key_path)) {
        throw std::runtime_error("Private key file not found: " + config.key_path);
    }

    // 创建 SSL context（TLS 1.3）
    ctx_ = std::make_unique<ssl::context>(ssl::context::tlsv13);

    // 设置选项：禁用旧版本协议
    ctx_->set_options(
        ssl::context::default_workarounds |
        ssl::context::no_sslv2 |
        ssl::context::no_sslv3 |
        ssl::context::no_tlsv1 |
        ssl::context::no_tlsv1_1 |
        ssl::context::no_tlsv1_2 |  // 仅允许 TLS 1.3
        ssl::context::single_dh_use
    );

    // 加载证书链
    ctx_->use_certificate_chain_file(config.cert_path);
    spdlog::info("Loaded certificate: {}", config.cert_path);

    // 加载私钥
    ctx_->use_private_key_file(config.key_path, ssl::context::pem);
    spdlog::info("Loaded private key: {}", config.key_path);

    // 验证私钥与证书匹配（使用 OpenSSL API）
    if (SSL_CTX_check_private_key(ctx_->native_handle()) != 1) {
        throw std::runtime_error("Private key does not match certificate");
    }
    spdlog::info("Private key matches certificate");

    // 设置密码套件（TLS 1.3）
    SSL_CTX_set_ciphersuites(ctx_->native_handle(), config.cipher_suites.c_str());
    spdlog::info("Cipher suites: {}", config.cipher_suites);

    // 可选：客户端证书验证
    if (config.require_client_cert) {
        if (!std::filesystem::exists(config.ca_path)) {
            throw std::runtime_error("CA file not found: " + config.ca_path);
        }
        ctx_->set_verify_mode(ssl::verify_peer | ssl::verify_fail_if_no_peer_cert);
        ctx_->load_verify_file(config.ca_path);
        spdlog::info("Client certificate verification enabled (CA: {})", config.ca_path);
    }

    enabled_ = true;
    spdlog::info("TLS context initialized successfully");
}

ssl::context* TlsContext::GetContext() {
    return enabled_ ? ctx_.get() : nullptr;
}

} // namespace p2p::security
