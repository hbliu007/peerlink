# API 参考

PeerLink C++ API 详细参考。

---

## 核心类

### P2PClient

P2P 通信的主入口类。

```cpp
namespace peerlink {

class P2PClient {
public:
    // 构造函数
    explicit P2PClient(const Config& config);

    // 析构函数
    ~P2PClient();

    // === 连接管理 ===

    // 连接到远程节点
    // 返回一个 Future，在连接成功后解析为 Session
    Future<Session> connect(const std::string& peer_id,
                           const ConnectOptions& options = {});

    // 监听入站连接
    void listen(const ConnectionCallback& callback);

    // 断开连接
    void disconnect(const std::string& session_id);

    // 获取所有活动连接
    std::vector<Session> get_active_sessions() const;

    // === 发送/接收 ===

    // 发送数据到指定节点
    Future<void> send(const std::string& session_id,
                      const Bytes& data);

    // 批量发送数据
    Future<void> send_batch(const std::string& session_id,
                            const std::vector<Bytes>& data_batch);

    // 设置接收回调
    void set_receive_callback(const DataCallback& callback);

    // === 配置 ===

    // 更新配置
    void update_config(const Config& config);

    // 获取当前配置
    const Config& get_config() const;

    // === 状态查询 ===

    // 获取自己的 Peer ID
    std::string get_peer_id() const;

    // 获取 DID 文档
    DidDocument get_did_document() const;

    // 获取统计信息
    ClientStats get_stats() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}
```

---

### Session

表示一个 P2P 会话。

```cpp
namespace peerlink {

class Session {
public:
    // 会话状态
    enum class State {
        Connecting,    // 连接中
        Connected,     // 已连接
        Disconnecting, // 断开中
        Disconnected,  // 已断开
        Failed         // 连接失败
    };

    // 连接类型
    enum class ConnectionType {
        UDP_DIRECT,   // UDP 直连
        TCP_DIRECT,   // TCP 直连
        RELAY_UDP,    // UDP 中继
        RELAY_TCP     // TCP 中继
    };

    // === 基本操作 ===

    // 获取会话 ID
    std::string get_session_id() const;

    // 获取远程 Peer ID
    std::string get_remote_peer_id() const;

    // 获取会话状态
    State get_state() const;

    // 获取连接类型
    ConnectionType get_connection_type() const;

    // === 数据传输 ===

    // 发送数据
    Future<void> send(const Bytes& data);

    // 批量发送数据
    Future<void> send_batch(const std::vector<Bytes>& data_batch);

    // 设置接收回调
    void set_receive_callback(const DataCallback& callback);

    // 设置关闭回调
    void set_close_callback(const CloseCallback& callback);

    // === 会话控制 ===

    // 关闭会话
    void close();

    // 检查会话是否活跃
    bool is_active() const;

    // === 统计信息 ===

    // 获取会话统计
    SessionStats get_stats() const;

    // 测试延迟
    Future<std::chrono::milliseconds> ping();

private:
    std::string session_id_;
    std::unique_ptr<Transport> transport_;
    State state_;
    // ... 其他私有成员
};

}
```

---

### Config

配置类。

```cpp
namespace peerlink {

struct Config {
    // 服务器配置
    ServerConfig server;

    // 本地配置
    LocalConfig local;

    // 网络配置
    NetworkConfig network;

    // 安全配置
    SecurityConfig security;

    // 日志配置
    LoggingConfig logging;

    // 性能配置
    PerformanceConfig performance;

    // 加载配置文件
    static Config load_from_file(const std::string& path);

    // 加载环境变量
    void load_from_env();

    // 验证配置
    bool validate() const;
};

struct ServerConfig {
    std::string signaling_server = "wss://localhost:8443";
    std::string stun_server = "stun:localhost:3478";
    std::vector<std::string> relay_servers = {"localhost:443"};
};

struct LocalConfig {
    std::string listen_address = "0.0.0.0";
    uint16_t listen_port = 18888;
    std::string data_dir = "~/.peerlink/data";
};

struct NetworkConfig {
    struct NatConfig {
        bool udp_punching = true;
        bool tcp_punching = true;
        bool relay_fallback = true;
        int punch_timeout = 10;  // 秒
    } nat;

    struct TransportConfig {
        std::string preferred = "auto";  // "udp", "tcp", "auto"
        bool tls = true;
        std::string tls_verify = "strict";
    } transport;
};

}
```

---

## 工具函数

### Bytes

字节数据容器。

```cpp
namespace peerlink {

class Bytes {
public:
    // 构造函数
    Bytes();
    explicit Bytes(size_t size);
    Bytes(const void* data, size_t size);
    Bytes(const std::string& str);
    Bytes(std::initializer_list<uint8_t> init);

    // 访问元素
    uint8_t& operator[](size_t index);
    const uint8_t& operator[](size_t index) const;

    // 迭代器
    uint8_t* data();
    const uint8_t* data() const;
    size_t size() const;
    bool empty() const;

    // 修改
    void resize(size_t new_size);
    void reserve(size_t new_capacity);
    void clear();

    // 转换
    std::string to_string() const;
    static Bytes from_hex(const std::string& hex);
    std::string to_hex() const;
    static Bytes from_base64(const std::string& base64);
    std::string to_base64() const;
};

}
```

### Future

异步操作结果。

```cpp
namespace peerlink {

template<typename T>
class Future {
public:
    // 等待结果（阻塞）
    T get();

    // 等待结果，带超时
    template<typename Rep, typename Period>
    T get(const std::chrono::duration<Rep, Period>& timeout);

    // 设置回调
    void then(const Callback<T>& callback);

    // 检查是否就绪
    bool is_ready() const;

    // 取消操作
    bool cancel();
};

}
```

---

## 回调类型

### ConnectionCallback

连接回调。

```cpp
using ConnectionCallback = std::function<void(Session)>;
```

**示例**：

```cpp
client.listen([](Session session) {
    std::cout << "New connection from: "
              << session.get_remote_peer_id() << std::endl;

    session.set_receive_callback([](const Bytes& data) {
        std::cout << "Received: " << data.to_string() << std::endl;
    });
});
```

### DataCallback

数据接收回调。

```cpp
using DataCallback = std::function<void(const Bytes&)>;
```

### CloseCallback

会话关闭回调。

```cpp
using CloseCallback = std::function<void(const std::string& reason)>;
```

---

## 错误处理

### 异常类

```cpp
namespace peerlink {

// 基础异常类
class Exception : public std::runtime_error {
public:
    explicit Exception(const std::string& message);
};

// 连接异常
class ConnectionException : public Exception {
public:
    ConnectionException(const std::string& peer_id,
                       const std::string& reason);
};

// 超时异常
class TimeoutException : public Exception {
public:
    explicit TimeoutException(const std::string& operation);
};

// 认证异常
class AuthenticationException : public Exception {
public:
    AuthenticationException(const std::string& reason);
};

// 配置异常
class ConfigException : public Exception {
public:
    ConfigException(const std::string& key,
                   const std::string& reason);
};

}
```

### 错误处理示例

```cpp
try {
    auto session = client.connect(peer_id).get();
    session.send(data).get();
} catch (const ConnectionException& e) {
    std::cerr << "Connection failed: " << e.what() << std::endl;
} catch (const TimeoutException& e) {
    std::cerr << "Operation timed out: " << e.what() << std::endl;
} catch (const Exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
}
```

---

## 完整示例

### Echo 服务器

```cpp
#include <peerlink/peerlink.hpp>

int main() {
    // 加载配置
    auto config = peerlink::Config::load_from_file("~/.peerlink/config.yaml");

    // 创建客户端
    peerlink::P2PClient client(config);

    // 监听入站连接
    client.listen([](peerlink::Session session) {
        std::cout << "New connection from: "
                  << session.get_remote_peer_id() << std::endl;

        // Echo 回数据
        session.set_receive_callback([session](const peerlink::Bytes& data) {
            session.send(data).then([](auto) {
                std::cout << "Echoed " << data.size() << " bytes" << std::endl;
            });
        });
    });

    // 保持运行
    std::this_thread::sleep_for(std::chrono::hours(24));

    return 0;
}
```

### 客户端

```cpp
#include <peerlink/peerlink.hpp>

int main() {
    auto config = peerlink::Config::load_from_file("~/.peerlink/config.yaml");
    peerlink::P2PClient client(config);

    // 连接到服务器
    auto peer_id = "did:peer:1zQmWvQxTqbGvZGh7Yh4z7Yh4z7Yh4z7Yh4z7Yh4z7Yh4z7";
    auto session = client.connect(peer_id).get();

    std::cout << "Connected to: " << session.get_remote_peer_id() << std::endl;
    std::cout << "Connection type: " << static_cast<int>(session.get_connection_type()) << std::endl;

    // 发送数据
    std::string message = "Hello, PeerLink!";
    session.send(peerlink::Bytes(message)).get();

    // 接收响应
    session.set_receive_callback([](const peerlink::Bytes& data) {
        std::cout << "Received: " << data.to_string() << std::endl;
    });

    // 等待用户输入
    std::string input;
    while (std::getline(std::cin, input)) {
        session.send(peerlink::Bytes(input)).get();
    }

    return 0;
}
```

---

**下一步**: [协议参考](protocol.md) · [安全参考](security.md)
