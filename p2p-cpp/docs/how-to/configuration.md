# 配置指南

PeerLink 的详细配置选项说明。

---

## 配置文件位置

PeerLink 按以下顺序查找配置文件：

1. `~/.peerlink/config.yaml`（用户配置）
2. `/etc/peerlink/config.yaml`（系统配置）
3. 环境变量（`PEERLINK_*`）

---

## 完整配置示例

```yaml
# ~/.peerlink/config.yaml

# === 服务器配置 ===
server:
  # Signaling 服务器地址
  signaling: wss://peerlink.example.com:8443

  # STUN 服务器地址
  stun: stun:peerlink.example.com:3478

  # Relay 服务器地址（可选，多个服务器会自动负载均衡）
  relay:
    - relay1.example.com:443
    - relay2.example.com:443

# === 本地配置 ===
local:
  # 监听地址
  listen_address: 0.0.0.0

  # 监听端口
  listen_port: 18888

  # 数据目录
  data_dir: ~/.peerlink/data

# === 网络配置 ===
network:
  # NAT 穿透配置
  nat:
    # 是否启用 UDP 打洞
    udp_punching: true

    # 是否启用 TCP 打洞
    tcp_punching: true

    # 是否使用中继（直连失败时）
    relay_fallback: true

    # 打洞超时时间（秒）
    punch_timeout: 10

  # 传输配置
  transport:
    # 首选传输协议（udp/tcp/auto）
    preferred: auto

    # 是否启用 TLS
    tls: true

    # TLS 验证（strict/none）
    tls_verify: strict

# === 安全配置 ===
security:
  # 身份配置
  identity:
    # DID 文档路径（自动生成）
    did_file: ~/.peerlink/identity.json

    # 私钥路径（加密存储）
    key_file: ~/.peerlink/private_key.pem

  # 加密配置
  encryption:
    # 使用的加密算法
    cipher: AES-256-GCM

    # 密钥交换算法
    key_exchange: ECDH-P256

# === 日志配置 ===
logging:
  # 日志级别（debug/info/warn/error）
  level: info

  # 日志输出（stdout/file/syslog）
  output: stdout

  # 日志文件路径（当 output=file 时）
  file: ~/.peerlink/peerlink.log

  # 日志轮转
  rotation:
    # 最大文件大小（MB）
    max_size: 100

    # 保留的日志文件数量
    max_files: 10

# === 性能配置 ===
performance:
  # 内存限制（MB）
  memory_limit: 512

  # CPU 使用限制（0-1，1=100%）
  cpu_limit: 0.5

  # 连接池大小
  connection_pool: 100

  # 缓冲区大小（字节）
  buffer_size: 65536
```

---

## 环境变量

所有配置都可以通过环境变量覆盖：

| 环境变量 | 配置路径 | 示例 |
|---------|---------|------|
| `PEERLINK_SIGNALING_SERVER` | `server.signaling` | `wss://peerlink.example.com:8443` |
| `PEERLINK_STUN_SERVER` | `server.stun` | `stun:peerlink.example.com:3478` |
| `PEERLINK_RELAY_SERVER` | `server.relay` | `relay.example.com:443` |
| `PEERLINK_LISTEN_PORT` | `local.listen_port` | `18888` |
| `PEERLINK_DATA_DIR` | `local.data_dir` | `/var/lib/peerlink` |
| `PEERLINK_LOG_LEVEL` | `logging.level` | `debug` |
| `PEERLINK_LOG_FILE` | `logging.file` | `/var/log/peerlink.log` |

---

## 命令行选项

```bash
# 连接时指定选项
peerlink connect <peer-id> \
  --timeout 30 \
  --auto-reconnect \
  --keepalive 30s \
  --prefer-udp

# 守护进程选项
peerlink daemon \
  --config /path/to/config.yaml \
  --pid-file /var/run/peerlink.pid \
  --log-level debug
```

---

## NAT 穿透调优

### UDP 打洞

```yaml
network:
  nat:
    # UDP 打洞尝试次数
    udp_retries: 3

    # UDP 打洞间隔（毫秒）
    udp_interval: 100

    # UDP 端口范围（用于打洞）
    udp_port_range:
      min: 10000
      max: 20000
```

### TCP 打洞

```yaml
network:
  nat:
    # TCP 同时打开尝试次数
    tcp_retries: 2

    # TCP 连接超时（秒）
    tcp_timeout: 5
```

### 中继降级

```yaml
network:
  nat:
    # 中继服务器选择策略（round-robin/least-connections/random）
    relay_selection: round-robin

    # 中继连接超时（秒）
    relay_timeout: 10
```

---

## 性能调优

### 高吞吐量场景

```yaml
performance:
  # 增大缓冲区
  buffer_size: 262144  # 256KB

  # 增加连接池
  connection_pool: 500

  # 调整 TCP 参数
  tcp:
    # TCP 窗口大小
    window_size: 1048576  # 1MB

    # 是否启用 TCP_NODELAY
    no_delay: true
```

### 低延迟场景

```yaml
performance:
  # 减小缓冲区
  buffer_size: 16384  # 16KB

  # 启用低延迟模式
  low_latency: true

  network:
    transport:
      # 首选 UDP
      preferred: udp
```

---

## 安全配置

### 证书固定

```yaml
security:
  tls:
    # 证书固定（防止 MITM 攻击）
    pin_certificates:
      - "sha256/AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="
```

### 访问控制

```yaml
security:
  acl:
    # 允许连接的 Peer ID 列表
    allowed_peers:
      - did:peer:1zQmWvQxTqbGvZGh7Yh4z7Yh4z7Yh4z7Yh4z7Yh4z7Yh4z7
      - did:peer:1zAbCdEfGhIjKlMnOpQrStUvWxYz1234567890abcdef

    # 拒绝连接的 Peer ID 列表
    blocked_peers: []
```

---

## 日志配置

### 结构化日志

```yaml
logging:
  # 使用 JSON 格式（便于日志聚合）
  format: json

  # 包含的字段
  fields:
    - timestamp
    - level
    - message
    - peer_id
    - session_id
    - latency
```

### 日志级别调整

```yaml
logging:
  # 模块级别的日志配置
  modules:
    signaling: info
    transport: debug
    nat: warn
    security: error
```

---

## 高可用配置

### 多 Signaling 服务器

```yaml
server:
  signaling:
    - wss://sig1.example.com:8443
    - wss://sig2.example.com:8443
    - wss://sig3.example.com:8443
```

### 自动重连

```yaml
network:
  reconnect:
    # 是否自动重连
    enabled: true

    # 重连间隔（指数退避）
    interval:
      initial: 1s
      max: 60s
      multiplier: 2

    # 最大重试次数（0=无限）
    max_retries: 0
```

---

## 配置验证

```bash
# 验证配置文件
peerlink config validate

# 测试配置
peerlink config test

# 查看当前配置
peerlink config show
```

---

**下一步**: [性能优化](performance.md) · [故障排查](troubleshooting.md)
