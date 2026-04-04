# 安全模型

## 概述

PeerLink 采用多层安全架构，确保 P2P 通信的机密性、完整性和真实性。

## 安全层次

```
┌──────────────────────────────────────────────────┐
│           应用层安全                              │
│   DID 身份认证 | Ed25519 签名                     │
├──────────────────────────────────────────────────┤
│           协议层安全                              │
│   SignedEnvelope (libp2p RFC 0002)              │
├──────────────────────────────────────────────────┤
│           传输层安全                              │
│   TLS 1.3 | DTLS 1.3                             │
├──────────────────────────────────────────────────┤
│           网络层安全                              │
│   协议协商 | 版本兼容性                           │
├──────────────────────────────────────────────────┤
│           防护层安全                              │
│   Rate Limiting | DoS 防护                        │
└──────────────────────────────────────────────────┘
```

## DID 身份系统

### 什么是 DID？

DID (Decentralized Identifier) 是去中心化标识符，用于在 P2P 网络中唯一标识一个节点。

### DID 格式

```
did:key:z6MkhaXgBZDvotDkL5257faiztiGiC2QtKLGpbnnEGta2doKX
```

### DID 生成

```cpp
#include <sodium.h>

std::string GenerateDID() {
    // 1. 生成 Ed25519 密钥对
    unsigned char pk[crypto_sign_PUBLICKEYBYTES];
    unsigned char sk[crypto_sign_SECRETKEYBYTES];
    crypto_sign_keypair(pk, sk);

    // 2. 编码为 multicodec
    std::string multicodec = EncodeMulticodec(0xed, pk);

    // 3. Base58 编码
    std::string did = "did:key:z" + Base58Encode(multicodec);

    return did;
}
```

### DID 验证

```cpp
bool VerifyDID(const std::string& did) {
    // 1. 检查前缀
    if (did.substr(0, 9) != "did:key:z") {
        return false;
    }

    // 2. Base58 解码
    std::string decoded = Base58Decode(did.substr(9));

    // 3. 检查 multicodec
    if (decoded[0] != 0xed || decoded[1] != 0x01) {
        return false;
    }

    // 4. 检查长度 (Ed25519 公钥 32 字节)
    if (decoded.length() != 34) {
        return false;
    }

    return true;
}
```

## Ed25519 签名

### 为什么选择 Ed25519？

| 特性 | Ed25519 | RSA-2048 | ECDSA-P256 |
|------|---------|----------|------------|
| **密钥大小** | 32 字节 | 256 字节 | 64 字节 |
| **签名大小** | 64 字节 | 256 字节 | 64 字节 |
| **签名速度** | ⚡ 极快 | 🐌 慢 | 🚀 快 |
| **验证速度** | ⚡ 极快 | 🐌 慢 | 🚀 快 |
| **安全性** | 128-bit | 112-bit | 128-bit |

### 密钥生成

```cpp
struct KeyPair {
    std::vector<uint8_t> public_key;  // 32 字节
    std::vector<uint8_t> secret_key;  // 64 字节
};

KeyPair GenerateKeyPair() {
    KeyPair pair;
    pair.public_key.resize(crypto_sign_PUBLICKEYBYTES);
    pair.secret_key.resize(crypto_sign_SECRETKEYBYTES);

    crypto_sign_keypair(
        pair.public_key.data(),
        pair.secret_key.data()
    );

    return pair;
}
```

### 签名

```cpp
std::vector<uint8_t> Sign(
    const std::vector<uint8_t>& message,
    const std::vector<uint8_t>& secret_key
) {
    std::vector<uint8_t> signature(crypto_sign_BYTES);
    unsigned long long sig_len;

    crypto_sign_detached(
        signature.data(),
        &sig_len,
        message.data(),
        message.size(),
        secret_key.data()
    );

    signature.resize(sig_len);
    return signature;
}
```

### 验证

```cpp
bool Verify(
    const std::vector<uint8_t>& message,
    const std::vector<uint8_t>& signature,
    const std::vector<uint8_t>& public_key
) {
    return crypto_sign_verify_detached(
        signature.data(),
        message.data(),
        message.size(),
        public_key.data()
    ) == 0;
}
```

## SignedEnvelope

### 什么是 SignedEnvelope？

SignedEnvelope 是 libp2p 的标准封装格式（RFC 0002），用于在 P2P 网络中传输已签名的消息。

### 结构

```protobuf
message SignedEnvelope {
    bytes public_key;       // 发送者的公钥
    bytes signature;        // Ed25519 签名
    bytes payload;          // 实际消息
    uint64 timestamp;       // 时间戳
}
```

### 封装

```cpp
SignedEnvelope CreateEnvelope(
    const std::vector<uint8_t>& payload,
    const std::vector<uint8_t>& secret_key,
    const std::vector<uint8_t>& public_key
) {
    // 1. 准备待签名数据
    std::vector<uint8_t> to_sign;
    to_sign.insert(to_sign.end(), public_key.begin(), public_key.end());
    to_sign.insert(to_sign.end(), payload.begin(), payload.end());

    // 2. 添加时间戳
    uint64_t timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    auto ts_bytes = reinterpret_cast<const uint8_t*>(&timestamp);
    to_sign.insert(to_sign.end(), ts_bytes, ts_bytes + sizeof(timestamp));

    // 3. 签名
    auto signature = Sign(to_sign, secret_key);

    // 4. 封装
    SignedEnvelope envelope;
    envelope.public_key = public_key;
    envelope.signature = signature;
    envelope.payload = payload;
    envelope.timestamp = timestamp;

    return envelope;
}
```

### 解封和验证

```cpp
std::optional<std::vector<uint8_t>> OpenEnvelope(
    const SignedEnvelope& envelope
) {
    // 1. 检查时间戳（防重放）
    uint64_t now = std::chrono::system_clock::now().time_since_epoch().count();
    if (now - envelope.timestamp > 30000000000) {  // 30 秒
        return std::nullopt;  // 过期
    }

    // 2. 准备验证数据
    std::vector<uint8_t> to_verify;
    to_verify.insert(to_verify.end(),
        envelope.public_key.begin(),
        envelope.public_key.end()
    );
    to_verify.insert(to_verify.end(),
        envelope.payload.begin(),
        envelope.payload.end()
    );
    auto ts_bytes = reinterpret_cast<const uint8_t*>(&envelope.timestamp);
    to_verify.insert(to_verify.end(),
        ts_bytes,
        ts_bytes + sizeof(envelope.timestamp)
    );

    // 3. 验证签名
    if (!Verify(to_verify, envelope.signature, envelope.public_key)) {
        return std::nullopt;  // 签名无效
    }

    return envelope.payload;
}
```

## TLS 1.3 加密

### 为什么需要 TLS？

即使有 Ed25519 签名，仍需 TLS 保护：

1. **机密性**: 防止中间人窃听
2. **完整性**: 防止数据篡改
3. **前向保密**: 密钥泄露不影响历史通信

### TLS 1.3 握手

```
Client                                    Server
  │                                         │
  │──── ClientHello (密钥共享) ────────────→│
  │                                         │
  │←── ServerHello (密钥共享) ─────────────│
  │←── EncryptedExtensions (证书) ─────────│
  │←── Finished (握手完成) ─────────────────│
  │                                         │
  │──── Finished (握手完成) ───────────────→│
  │                                         │
  │     应用数据 (加密) ↕                     │
```

### 配置

```cpp
#include <openssl/ssl.h>

SSL_CTX* CreateTLSContext() {
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());

    // 强制 TLS 1.3
    SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);
    SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION);

    // 验证证书
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);

    // 加密套件
    SSL_CTX_set_cipher_list(ctx,
        "TLS_AES_128_GCM_SHA256:"
        "TLS_AES_256_GCM_SHA384:"
        "TLS_CHACHA20_POLY1305_SHA256"
    );

    return ctx;
}
```

### DTLS for UDP

```cpp
// UDP 传输使用 DTLS
SSL_CTX* CreateDTLSContext() {
    SSL_CTX* ctx = SSL_CTX_new(DTLS_client_method());

    SSL_CTX_set_min_proto_version(ctx, DTLS1_3_VERSION);
    SSL_CTX_set_max_proto_version(ctx, DTLS1_3_VERSION);

    // DTLS 特定配置
    SSL_CTX_set_read_ahead(ctx, 1);  // 缓冲重传数据

    return ctx;
}
```

## 协议协商

### 协议版本

```cpp
struct ProtocolVersion {
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
};

constexpr ProtocolVersion CURRENT_VERSION = {3, 1, 0};
```

### 版本兼容性

```
| Client  | Server  | Result          |
|---------|---------|-----------------|
| 3.1.0   | 3.0.0   | ✅ 向后兼容     |
| 3.0.0   | 3.1.0   | ⚠️ 部分功能    |
| 2.0.0   | 3.0.0   | ❌ 不兼容       |
```

### 协商流程

```cpp
std::optional<ProtocolVersion> NegotiateVersion(
    const std::vector<ProtocolVersion>& client_versions,
    const std::vector<ProtocolVersion>& server_versions
) {
    // 找到共同支持的最高版本
    for (const auto& cv : client_versions) {
        for (const auto& sv : server_versions) {
            if (cv.major == sv.major && cv.minor == sv.minor) {
                return cv;  // 匹配
            }
        }
    }
    return std::nullopt;  // 无共同版本
}
```

## Rate Limiting

### Token Bucket 算法

```cpp
class TokenBucket {
public:
    TokenBucket(size_t rate_per_sec, size_t burst)
        : rate_(rate_per_sec)
        , burst_(burst)
        , tokens_(burst)
        , last_refill_(std::chrono::steady_clock::now()) {}

    bool TryConsume(size_t tokens = 1) {
        Refill();

        std::lock_guard<std::mutex> lock(mutex_);
        if (tokens_ >= tokens) {
            tokens_ -= tokens;
            return true;
        }
        return false;
    }

private:
    void Refill() {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_refill_
        ).count();

        size_t new_tokens = (elapsed * rate_) / 1000;
        tokens_ = std::min(burst_, tokens_ + new_tokens);
        last_refill_ = now;
    }

    size_t rate_;
    size_t burst_;
    size_t tokens_;
    std::chrono::steady_clock::time_point last_refill_;
    std::mutex mutex_;
};
```

### 应用

```cpp
class P2PClient {
public:
    void SendData(const std::vector<uint8_t>& data) {
        // 发送限流
        if (!send_rate_limiter_.TryConsume(data.size())) {
            // 超过限制，延迟或拒绝
            return;
        }

        transport_->send(data);
    }

    void OnReceived(const std::vector<uint8_t>& data) {
        // 接收限流
        if (!recv_rate_limiter_.TryConsume(data.size())) {
            // 可能是攻击，断开连接
            Close();
            return;
        }

        // 处理数据
        HandleMessage(data);
    }

private:
    TokenBucket send_rate_limiter_{10'000'000, 50'000'000};  // 10MB/s, 突发 50MB
    TokenBucket recv_rate_limiter_{10'000'000, 50'000'000};
};
```

## DoS 防护

### 连接限制

```cpp
class ConnectionLimiter {
public:
    bool AllowConnection(const std::string& ip) {
        std::lock_guard<std::mutex> lock(mutex_);

        // 检查单 IP 连接数
        auto& count = ip_connections_[ip];
        if (count >= kMaxConnectionsPerIP) {
            return false;
        }

        // 检查总连接数
        if (total_connections_ >= kMaxTotalConnections) {
            return false;
        }

        ++count;
        ++total_connections_;
        return true;
    }

    void RemoveConnection(const std::string& ip) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = ip_connections_.find(ip);
        if (it != ip_connections_.end()) {
            --(it->second);
            if (it->second == 0) {
                ip_connections_.erase(it);
            }
        }
        --total_connections_;
    }

private:
    static constexpr size_t kMaxConnectionsPerIP = 10;
    static constexpr size_t kMaxTotalConnections = 10000;

    std::unordered_map<std::string, size_t> ip_connections_;
    size_t total_connections_ = 0;
    std::mutex mutex_;
};
```

### 黑名单

```cpp
class Blacklist {
public:
    void Block(const std::string& ip, std::chrono::seconds duration) {
        std::lock_guard<std::mutex> lock(mutex_);
        blocked_[ip] = std::chrono::steady_clock::now() + duration;
    }

    bool IsBlocked(const std::string& ip) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = blocked_.find(ip);
        if (it == blocked_.end()) {
            return false;
        }

        if (std::chrono::steady_clock::now() > it->second) {
            blocked_.erase(it);  // 过期移除
            return false;
        }

        return true;
    }

private:
    std::unordered_map<std::string,
        std::chrono::steady_clock::time_point> blocked_;
    std::mutex mutex_;
};
```

## 密钥管理

### 密钥存储

```cpp
class KeyStore {
public:
    bool LoadKey(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return false;
        }

        std::vector<uint8_t> key(64);
        file.read(reinterpret_cast<char*>(key.data()), key.size());

        secret_key_ = key;
        return true;
    }

    bool GenerateAndSave(const std::string& path) {
        // 生成新密钥
        auto pair = GenerateKeyPair();
        secret_key_ = pair.secret_key;

        // 保存到文件（权限 600）
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(
            secret_key_.data()), secret_key_.size()
        );

        #ifdef __unix__
        chmod(path.c_str(), 0600);
        #endif

        return true;
    }

private:
    std::vector<uint8_t> secret_key_;
};
```

### 硬件安全模块 (HSM)

```cpp
// 可选：使用 HSM 存储密钥
class HSMKeyStore {
public:
    std::vector<uint8_t> SignWithHSM(
        const std::vector<uint8_t>& message
    ) {
        // 调用 HSM API
        return hsm_->Sign(message);
    }

private:
    std::unique_ptr<HSMDevice> hsm_;
};
```

## 安全检查清单

在部署 PeerLink 应用前，请确保：

- [ ] 所有通信都使用 TLS 1.3 加密
- [ ] 所有消息都使用 Ed25519 签名
- [ ] DID 验证已启用
- [ ] Rate Limiting 已配置
- [ ] 连接限制已设置
- [ ] 密钥文件权限正确（600）
- [ ] 证书有效（如果使用 mTLS）
- [ ] 定期更新依赖库（OpenSSL, libsodium）

## 参考资料

- [libp2p RFC 0002 - Peer Identification](https://github.com/libp2p/specs/blob/master/peer-ids/README.md)
- [libp2p RFC 0003 - Envelopes](https://github.com/libp2p/specs/blob/master/envelopes/envelopes.md)
- [RFC 8446 - TLS 1.3](https://tools.ietf.org/html/rfc8446)
- [Ed25519 - High-speed high-security signatures](https://ed25519.cr.yp.to/)
- [libsodium - Documentation](https://libsodium.gitbook.io/doc/)
