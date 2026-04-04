---
title: 配置参考
description: 完整的TOML配置文件参考，包含所有字段、类型、默认值和说明
tags:
  - configuration
  - toml
  - reference
  - settings
---

# 配置参考

本页面提供PeerLink TOML配置文件的完整参考。

## 配置文件格式

PeerLink使用TOML格式配置文件。配置文件位置：

1. 通过环境变量指定：`PEERLINK_CONFIG=/path/to/config.toml`
2. 默认位置：`/etc/peerlink/peerlink.toml`

## 配置结构

```toml
# [server] 服务器通用配置
# [signaling] 信令服务器配置
# [did] DID服务器配置
# [stun] STUN服务器配置
# [turn] TURN中继服务器配置
# [relay] Relay服务器配置
# [tls] TLS配置
# [admin] Admin HTTP配置
# [rate_limit] 限速配置
# [logging] 日志配置
```

## [server] 服务器通用配置

| 字段路径 | 类型 | 默认值 | 必填 | 说明 | 示例 |
|---------|------|--------|------|------|------|
| `server.host` | string | `"0.0.0.0"` | 否 | 监听地址 | `"0.0.0.0"` |
| `server.port` | int | `8080` | 否 | 监听端口 | `8080` |
| `server.worker_threads` | int | `0` | 否 | 工作线程数（0=CPU核心数） | `4` |
| `server.max_connections` | int | `1000` | 否 | 最大连接数 | `5000` |

**示例：**

```toml
[server]
host = "0.0.0.0"
port = 8080
worker_threads = 4
max_connections = 5000
```

## [signaling] 信令服务器配置

| 字段路径 | 类型 | 默认值 | 必填 | 说明 | 示例 |
|---------|------|--------|------|------|------|
| `signaling.host` | string | `"0.0.0.0"` | 否 | 监听地址 | `"0.0.0.0"` |
| `signaling.port` | int | `8080` | 否 | 监听端口 | `18080` |
| `signaling.heartbeat_interval` | int | `30` | 否 | 心跳间隔（秒） | `30` |
| `signaling.connection_timeout` | int | `90` | 否 | 连接超时（秒） | `90` |
| `signaling.allow_insecure_registration` | bool | `false` | 否 | 允许不安全注册 | `false` |
| `signaling.turn_public_ip` | string | `"127.0.0.1"` | 否 | TURN公网IP | `"<YOUR_SERVER_IP>"` |
| `signaling.turn_port` | int | `9001` | 否 | TURN端口 | `9001` |
| `signaling.jwt_secret` | string | - | 是 | JWT密钥 | `"your-secret"` |
| `signaling.redis_host` | string | `"127.0.0.1"` | 否 | Redis主机 | `"127.0.0.1"` |
| `signaling.redis_port` | int | `6379` | 否 | Redis端口 | `6379` |
| `signaling.max_connections` | int | `10000` | 否 | 最大连接数 | `10000` |

**示例：**

```toml
[signaling]
host = "127.0.0.1"
port = 18080
heartbeat_interval = 30
connection_timeout = 90
allow_insecure_registration = false
turn_public_ip = "<YOUR_SERVER_IP>"
turn_port = 9001
jwt_secret = "your-jwt-secret-here"
```

## [did] DID服务器配置

| 字段路径 | 类型 | 默认值 | 必填 | 说明 | 示例 |
|---------|------|--------|------|------|------|
| `did.host` | string | `"0.0.0.0"` | 否 | 监听地址 | `"0.0.0.0"` |
| `did.port` | int | `8081` | 否 | 监听端口 | `18081` |
| `did.redis_host` | string | `"127.0.0.1"` | 否 | Redis主机 | `"127.0.0.1"` |
| `did.redis_port` | int | `6379` | 否 | Redis端口 | `6379` |
| `did.jwt_secret` | string | - | 是 | JWT密钥 | `"your-secret"` |
| `did.max_connections` | int | `1000` | 否 | 最大连接数 | `1000` |

**示例：**

```toml
[did]
host = "127.0.0.1"
port = 18081
redis_host = "127.0.0.1"
redis_port = 6379
jwt_secret = "your-jwt-secret-here"
max_connections = 1000
```

## [stun] STUN服务器配置

| 字段路径 | 类型 | 默认值 | 必填 | 说明 | 示例 |
|---------|------|--------|------|------|------|
| `stun.host` | string | `"0.0.0.0"` | 否 | 监听地址 | `"0.0.0.0"` |
| `stun.udp_port` | int | `3478` | 否 | UDP端口 | `3478` |
| `stun.tcp_port` | int | `3479` | 否 | TCP端口 | `3479` |

**示例：**

```toml
[stun]
host = "0.0.0.0"
udp_port = 3478
tcp_port = 3479
```

## [turn] TURN中继服务器配置

| 字段路径 | 类型 | 默认值 | 必填 | 说明 | 示例 |
|---------|------|--------|------|------|------|
| `turn.host` | string | `"0.0.0.0"` | 否 | 监听地址 | `"0.0.0.0"` |
| `turn.port` | int | `9001` | 否 | 控制端口 | `9001` |
| `turn.public_ip` | string | `"127.0.0.1"` | 是 | 公网IP | `"<YOUR_SERVER_IP>"` |
| `turn.min_port` | int | `50000` | 否 | 中继端口范围最小值 | `50000` |
| `turn.max_port` | int | `50100` | 否 | 中继端口范围最大值 | `50100` |
| `turn.lifetime` | int | `600` | 否 | 分配生命周期（秒） | `600` |
| `turn.max_allocations` | int | `1000` | 否 | 最大分配数 | `1000` |
| `turn.bandwidth_limit_bytes` | int | `1048576` | 否 | 带宽限制（字节/秒） | `1048576` |

**示例：**

```toml
[turn]
host = "0.0.0.0"
port = 9001
public_ip = "<YOUR_SERVER_IP>"
min_port = 50000
max_port = 50100
lifetime = 600
max_allocations = 1000
bandwidth_limit_bytes = 1048576  # 1MB/s
```

## [relay] Relay服务器配置

| 字段路径 | 类型 | 默认值 | 必填 | 说明 | 示例 |
|---------|------|--------|------|------|------|
| `relay.enabled` | bool | `true` | 否 | 启用Relay | `true` |
| `relay.host` | string | `"0.0.0.0"` | 否 | 监听地址 | `"127.0.0.1"` |
| `relay.port` | int | `9700` | 否 | 监听端口 | `9700` |
| `relay.max_clients` | int | `1000` | 否 | 最大客户端数 | `1000` |
| `relay.timeout` | int | `300` | 否 | 客户端超时（秒） | `300` |

**示例：**

```toml
[relay]
enabled = true
host = "127.0.0.1"
port = 9700
max_clients = 1000
timeout = 300
```

## [tls] TLS配置

| 字段路径 | 类型 | 默认值 | 必填 | 说明 | 示例 |
|---------|------|--------|------|------|------|
| `tls.enabled` | bool | `false` | 否 | 启用TLS | `true` |
| `tls.cert_path` | string | - | 否 | 证书路径 | `"/etc/letsencrypt/..."` |
| `tls.key_path` | string | - | 否 | 私钥路径 | `"/etc/letsencrypt/..."` |
| `tls.ca_path` | string | - | 否 | CA证书路径 | `"/etc/ssl/certs/..."` |
| `tls.cipher_suites` | string | - | 否 | 密码套件 | `"TLS_AES_256_GCM_SHA384:..."` |
| `tls.require_client_cert` | bool | `false` | 否 | 要求客户端证书 | `false` |
| `tls.min_tls_version` | string | `"1.2"` | 否 | 最低TLS版本 | `"1.3"` |
| `tls.max_tls_version` | string | `"1.3"` | 否 | 最高TLS版本 | `"1.3"` |
| `tls.handshake_timeout_ms` | int | `10000` | 否 | 握手超时（毫秒） | `10000` |
| `tls.hsts_enabled` | bool | `false` | 否 | 启用HSTS | `true` |
| `tls.hsts_max_age` | int | `31536000` | 否 | HSTS最大年龄 | `31536000` |
| `tls.ocsp_stapling` | bool | `false` | 否 | 启用OCSP Stapling | `true` |

**示例：**

```toml
[signaling.tls]
enabled = true
cert_path = "/etc/letsencrypt/live/example.com/fullchain.pem"
key_path = "/etc/letsencrypt/live/example.com/privkey.pem"
min_tls_version = "1.3"
max_tls_version = "1.3"
cipher_suites = "TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256"
hsts_enabled = true
hsts_max_age = 31536000
ocsp_stapling = true
```

## [admin] Admin HTTP配置

| 字段路径 | 类型 | 默认值 | 必填 | 说明 | 示例 |
|---------|------|--------|------|------|------|
| `admin.enabled` | bool | `true` | 否 | 启用Admin HTTP | `true` |
| `admin.host` | string | `"127.0.0.1"` | 否 | 监听地址 | `"127.0.0.1"` |
| `admin.port` | int | `9300` | 否 | 监听端口 | `9300` |

**服务特定Admin配置：**

| 字段路径 | 类型 | 默认值 | 说明 |
|---------|------|--------|------|
| `gateway.admin.enabled` | bool | `true` | Gateway Admin启用 |
| `gateway.admin.host` | string | `"127.0.0.1"` | Gateway Admin地址 |
| `gateway.admin.port` | int | `9300` | Gateway Admin端口 |
| `network.admin.enabled` | bool | `true` | Network Admin启用 |
| `network.admin.host` | string | `"127.0.0.1"` | Network Admin地址 |
| `network.admin.port` | int | `9301` | Network Admin端口 |
| `signaling.admin.enabled` | bool | `true` | Signaling Admin启用 |
| `signaling.admin.host` | string | `"127.0.0.1"` | Signaling Admin地址 |
| `signaling.admin.port` | int | `9302` | Signaling Admin端口 |
| `did.admin.enabled` | bool | `true` | DID Admin启用 |
| `did.admin.host` | string | `"127.0.0.1"` | DID Admin地址 |
| `did.admin.port` | int | `9303` | DID Admin端口 |

**示例：**

```toml
[gateway.admin]
enabled = true
host = "127.0.0.1"
port = 9300

[network.admin]
enabled = true
host = "127.0.0.1"
port = 9301
```

## [rate_limit] 限速配置

| 字段路径 | 类型 | 默认值 | 必填 | 说明 | 示例 |
|---------|------|--------|------|------|------|
| `rate_limit.requests_per_second` | int | `10` | 否 | 每秒请求数 | `20` |
| `rate_limit.burst_size` | int | `20` | 否 | 突发大小 | `50` |
| `rate_limit.ban_threshold` | int | `5` | 否 | 封禁阈值 | `5` |
| `rate_limit.ban_duration_seconds` | int | `60` | 否 | 封禁时长（秒） | `300` |

**服务特定限速配置：**

```toml
[signaling.rate_limit]
requests_per_second = 20
burst_size = 50
ban_threshold = 5
ban_duration_seconds = 300

[did.rate_limit]
requests_per_second = 10
burst_size = 20
ban_threshold = 5
ban_duration_seconds = 300
```

## [logging] 日志配置

| 字段路径 | 类型 | 默认值 | 必填 | 说明 | 示例 |
|---------|------|--------|------|------|------|
| `logging.level` | string | `"info"` | 否 | 日志级别 | `"info"` |
| `logging.file` | string | - | 否 | 日志文件路径 | `"/var/log/peerlink/..."` |
| `logging.max_size` | string | `"100M"` | 否 | 最大文件大小 | `"100M"` |
| `logging.max_files` | int | `10` | 否 | 最大文件数 | `10` |
| `logging.format` | string | `"text"` | 否 | 日志格式 | `"json"` |

**日志级别：** `trace`, `debug`, `info`, `warn`, `error`, `fatal`

**示例：**

```toml
[logging]
level = "info"
file = "/var/log/peerlink/peerlink.log"
max_size = "100M"
max_files = 10
format = "text"
```

## [redis] Redis配置

| 字段路径 | 类型 | 默认值 | 必填 | 说明 | 示例 |
|---------|------|--------|------|------|------|
| `redis.host` | string | `"127.0.0.1"` | 否 | Redis主机 | `"127.0.0.1"` |
| `redis.port` | int | `6379` | 否 | Redis端口 | `6379` |
| `redis.password` | string | - | 否 | Redis密码 | `"your-password"` |
| `redis.db` | int | `0` | 否 | Redis数据库 | `0` |
| `redis.pool_size` | int | `10` | 否 | 连接池大小 | `10` |

**示例：**

```toml
[redis]
host = "127.0.0.1"
port = 6379
password = "your-redis-password"
db = 0
pool_size = 10
```

## [auth] 认证配置

| 字段路径 | 类型 | 默认值 | 必填 | 说明 | 示例 |
|---------|------|--------|------|------|------|
| `auth.jwt_secret` | string | - | 是 | JWT密钥 | `"your-secret"` |
| `auth.token_expiry` | int | `3600` | 否 | Token过期时间（秒） | `3600` |
| `auth.refresh_token_expiry` | int | `86400` | 否 | 刷新Token过期时间 | `86400` |

**示例：**

```toml
[auth]
jwt_secret = "your-jwt-secret-here"
token_expiry = 3600
refresh_token_expiry = 86400
```

## [network] 网络服务配置

| 字段路径 | 类型 | 默认值 | 必填 | 说明 | 示例 |
|---------|------|--------|------|------|------|
| `network.metrics_interval_seconds` | int | `30` | 否 | 指标采集间隔 | `30` |

## 环境变量覆盖

所有配置字段都可以通过环境变量覆盖：

```bash
# 格式：<SECTION>_<FIELD> (全大写)
export SIGNALING_PORT=8443
export JWT_SECRET="my-secret"
export TURN_PUBLIC_IP="<YOUR_SERVER_IP>"

# 使用配置文件
export PEERLINK_CONFIG=/etc/peerlink/peerlink.toml
```

## 完整配置示例

```toml
# 信令服务器
[signaling]
host = "127.0.0.1"
port = 18080
heartbeat_interval = 30
connection_timeout = 90
allow_insecure_registration = false

# DID服务器
[did]
host = "127.0.0.1"
port = 18081
max_connections = 1000

# Redis
[redis]
host = "127.0.0.1"
port = 6379

# 认证
[auth]
jwt_secret = "change-me-in-production"

# STUN
[stun]
host = "0.0.0.0"
udp_port = 3478
tcp_port = 3479

# TURN
[turn]
host = "0.0.0.0"
port = 9001
public_ip = "<YOUR_SERVER_IP>"
min_port = 50000
max_port = 50100
lifetime = 600
max_allocations = 1000

# 网络服务
[network]
metrics_interval_seconds = 30

# Gateway Admin
[gateway.admin]
enabled = true
host = "127.0.0.1"
port = 9300

# Network Admin
[network.admin]
enabled = true
host = "127.0.0.1"
port = 9301

# Signaling Admin
[signaling.admin]
enabled = true
host = "127.0.0.1"
port = 9302

# DID Admin
[did.admin]
enabled = true
host = "127.0.0.1"
port = 9303
```
