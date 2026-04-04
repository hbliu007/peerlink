#pragma once

#include "p2p/utils/service_config.hpp"
#include <boost/asio/ssl.hpp>
#include <memory>

namespace ssl = boost::asio::ssl;

namespace p2p::security {

using TlsConfig = p2p::utils::TlsConfig;

class TlsContext {
public:
    // 单例模式
    static TlsContext& GetInstance();

    // 初始化 TLS context
    void Initialize(const TlsConfig& config);

    // 获取 SSL context（如果未启用返回 nullptr）
    ssl::context* GetContext();

    // 检查是否启用 TLS
    bool IsEnabled() const { return enabled_; }

    // 获取配置
    const TlsConfig& GetConfig() const { return config_; }

private:
    TlsContext() = default;
    ~TlsContext() = default;
    TlsContext(const TlsContext&) = delete;
    TlsContext& operator=(const TlsContext&) = delete;

    std::unique_ptr<ssl::context> ctx_;
    TlsConfig config_;
    bool enabled_ = false;
};

} // namespace p2p::security
