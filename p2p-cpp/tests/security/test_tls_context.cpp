#include "p2p/security/tls_context.hpp"
#include "p2p/utils/service_config.hpp"
#include <iostream>
#include <spdlog/spdlog.h>

int main() {
    spdlog::set_level(spdlog::level::info);

    try {
        // 测试 1: TLS 禁用
        std::cout << "Test 1: TLS disabled\n";
        p2p::utils::TlsConfig config1;
        config1.enabled = false;

        auto& ctx1 = p2p::security::TlsContext::GetInstance();
        ctx1.Initialize(config1);

        if (!ctx1.IsEnabled()) {
            std::cout << "✓ TLS correctly disabled\n";
        } else {
            std::cout << "✗ TLS should be disabled\n";
            return 1;
        }

        // 测试 2: TLS 启用（使用生成的证书）
        std::cout << "\nTest 2: TLS enabled with valid certificates\n";
        p2p::utils::TlsConfig config2;
        config2.enabled = true;
        config2.cert_path = "certs/server.crt";
        config2.key_path = "certs/server.key";

        ctx1.Initialize(config2);

        if (ctx1.IsEnabled() && ctx1.GetContext() != nullptr) {
            std::cout << "✓ TLS correctly enabled\n";
            std::cout << "✓ SSL context created\n";
        } else {
            std::cout << "✗ TLS initialization failed\n";
            return 1;
        }

        std::cout << "\nAll tests passed!\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
