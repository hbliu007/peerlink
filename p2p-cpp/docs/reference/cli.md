---
title: CLI 命令参考
description: PeerLink命令行工具完整参考，包括relay-tunnel、signaling-server、relay-server、did-server
tags:
  - cli
  - reference
  - commands
  - tools
---

# CLI 命令参考

本页面提供PeerLink命令行工具的完整参考。

## relay-tunnel

轻量级TCP隧道工具，用于通过Relay服务器建立端到端连接。

### server 子命令

在服务器端运行，将relay流量转发到本地服务（如SSH）。

```bash
relay-tunnel server [OPTIONS]
```

**选项：**

| 选项 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `--did <DID>` | string | - | 设备ID（必填） |
| `--relay <HOST:PORT>` | string | - | Relay服务器地址 |
| `--forward <HOST:PORT>` | string | `127.0.0.1:22` | 转发目标地址 |
| `--relay-mode <mode>` | string | `auto` | 连接模式 |

**连接模式：**
- `auto` - 自动选择最佳方式
- `p2p-only` - 仅使用P2P
- `relay-only` - 强制使用中继

**示例：**

```bash
# 办公机端：转发到本地SSH
relay-tunnel server \
    --did office-001 \
    --relay <YOUR_SERVER_IP>:9443 \
    --forward 127.0.0.1:22

# 使用中继模式
relay-tunnel server \
    --did office-001 \
    --relay <YOUR_SERVER_IP>:9443 \
    --forward 127.0.0.1:22 \
    --relay-mode relay-only
```

---

### client 子命令

在客户端运行，监听本地端口并将流量通过relay转发到对端。

```bash
relay-tunnel client [OPTIONS]
```

**选项：**

| 选项 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `--did <DID>` | string | - | 本地设备ID（必填） |
| `--target <TARGET_DID>` | string | - | 目标设备ID（必填） |
| `--relay <HOST:PORT>` | string | - | Relay服务器地址 |
| `--listen <PORT>` | int | `9022` | 本地监听端口 |
| `--listen-addr <ADDR>` | string | `127.0.0.1` | 监听地址 |
| `--relay-mode <mode>` | string | `auto` | 连接模式 |

**示例：**

```bash
# 家里端：连接办公机
relay-tunnel client \
    --did home-mac \
    --target office-001 \
    --relay <YOUR_SERVER_IP>:9443 \
    --listen 9022

# 监听所有接口（允许手机连接）
relay-tunnel client \
    --did home-mac \
    --target office-001 \
    --relay <YOUR_SERVER_IP>:9443 \
    --listen 9022 \
    --listen-addr 0.0.0.0
```

---

### 环境变量

```bash
# Relay服务器地址
RELAY_SERVER=<YOUR_SERVER_IP>:9443

# 连接模式
RELAY_MODE=relay-only

# 日志级别
RUST_LOG=debug
```

## signaling-server

信令服务器，负责P2P连接建立和信令交换。

### 启动参数

```bash
p2p-signaling-server [OPTIONS]
```

**选项：**

| 选项 | 环境变量 | 默认值 | 说明 |
|------|----------|--------|------|
| `--config <PATH>` | `PEERLINK_CONFIG` | - | 配置文件路径 |
| `--host <ADDR>` | `SIGNALING_HOST` | `0.0.0.0` | 监听地址 |
| `--port <PORT>` | `SIGNALING_PORT` | `8080` | 监听端口 |
| `--jwt-secret <SECRET>` | `JWT_SECRET` | - | JWT密钥 |
| `--allow-insecure` | `SIGNALING_ALLOW_INSECURE_REGISTRATION` | `false` | 允许不安全注册 |

**示例：**

```bash
# 使用配置文件
p2p-signaling-server --config /etc/peerlink/peerlink.toml

# 使用命令行参数
p2p-signaling-server \
    --host 0.0.0.0 \
    --port 8080 \
    --jwt-secret "your-secret"

# 使用环境变量
export SIGNALING_PORT=8443
export JWT_SECRET="your-secret"
p2p-signaling-server
```

---

### TLS选项

| 选项 | 环境变量 | 默认值 | 说明 |
|------|----------|--------|------|
| `--tls-enabled` | `SIGNALING_TLS_ENABLED` | `false` | 启用TLS |
| `--tls-cert <PATH>` | `SIGNALING_TLS_CERT_PATH` | - | 证书路径 |
| `--tls-key <PATH>` | `SIGNALING_TLS_KEY_PATH` | - | 私钥路径 |

**示例：**

```bash
p2p-signaling-server \
    --tls-enabled \
    --tls-cert /etc/letsencrypt/live/example.com/fullchain.pem \
    --tls-key /etc/letsencrypt/live/example.com/privkey.pem
```

---

### 限速选项

| 选项 | 环境变量 | 默认值 | 说明 |
|------|----------|--------|------|
| `--rate-limit-rps <N>` | `SIGNALING_RATE_LIMIT_RPS` | `20` | 每秒请求数 |
| `--rate-limit-burst <N>` | `SIGNALING_RATE_LIMIT_BURST` | `50` | 突发大小 |
| `--rate-limit-ban-threshold <N>` | `SIGNALING_RATE_LIMIT_BAN_THRESHOLD` | `5` | 封禁阈值 |
| `--rate-limit-ban-duration <N>` | `SIGNALING_RATE_LIMIT_BAN_DURATION` | `300` | 封禁时长（秒） |

## relay-server

TURN中继服务器，处理UDP打洞失败时的数据中继。

### 启动参数

```bash
p2p-relay-server [OPTIONS]
```

**选项：**

| 选项 | 环境变量 | 默认值 | 说明 |
|------|----------|--------|------|
| `--config <PATH>` | `PEERLINK_CONFIG` | - | 配置文件路径 |
| `--host <ADDR>` | `TURN_HOST` | `0.0.0.0` | 监听地址 |
| `--port <PORT>` | `TURN_PORT` | `9001` | 控制端口 |
| `--public-ip <IP>` | `TURN_PUBLIC_IP` | `127.0.0.1` | 公网IP |
| `--min-port <PORT>` | `TURN_MIN_PORT` | `50000` | 中继端口最小值 |
| `--max-port <PORT>` | `TURN_MAX_PORT` | `50100` | 中继端口最大值 |
| `--lifetime <SECONDS>` | `TURN_LIFETIME` | `600` | 分配生命周期 |
| `--max-allocations <N>` | `TURN_MAX_ALLOCATIONS` | `1000` | 最大分配数 |

**示例：**

```bash
# 使用配置文件
p2p-relay-server --config /etc/peerlink/peerlink.toml

# 使用命令行参数
p2p-relay-server \
    --host 0.0.0.0 \
    --port 9001 \
    --public-ip <YOUR_SERVER_IP> \
    --min-port 50000 \
    --max-port 50100
```

---

### 带宽限制

| 选项 | 环境变量 | 默认值 | 说明 |
|------|----------|--------|------|
| `--bandwidth-limit <BYTES>` | `TURN_BANDWIDTH_LIMIT` | `1048576` | 每秒带宽限制（字节） |

**示例：**

```bash
# 限制为1MB/s
p2p-relay-server --bandwidth-limit 1048576
```

## did-server

DID（去中心化标识）服务器，提供设备ID管理和查询服务。

### 启动参数

```bash
p2p-did-server [OPTIONS]
```

**选项：**

| 选项 | 环境变量 | 默认值 | 说明 |
|------|----------|--------|------|
| `--config <PATH>` | `PEERLINK_CONFIG` | - | 配置文件路径 |
| `--host <ADDR>` | `DID_HOST` | `0.0.0.0` | 监听地址 |
| `--port <PORT>` | `DID_PORT` | `8081` | 监听端口 |
| `--jwt-secret <SECRET>` | `JWT_SECRET` | - | JWT密钥 |
| `--max-connections <N>` | `DID_MAX_CONNECTIONS` | `1000` | 最大连接数 |
| `--redis-host <HOST>` | `REDIS_HOST` | `127.0.0.1` | Redis主机 |
| `--redis-port <PORT>` | `REDIS_PORT` | `6379` | Redis端口 |

**示例：**

```bash
# 使用配置文件
p2p-did-server --config /etc/peerlink/peerlink.toml

# 使用命令行参数
p2p-did-server \
    --host 0.0.0.0 \
    --port 8081 \
    --jwt-secret "your-secret" \
    --max-connections 1000
```

---

### TLS选项

与signaling-server相同。

## simple-relay-server

简单的TCP中继服务器，用于relay-tunnel工具。

### 启动参数

```bash
simple-relay-server [OPTIONS]
```

**选项：**

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `--host <ADDR>` | `0.0.0.0` | 监听地址 |
| `--port <PORT>` | `9700` | 监听端口 |
| `--max-clients <N>` | `1000` | 最大客户端数 |
| `--timeout <SECONDS>` | `300` | 客户端超时 |

**示例：**

```bash
# 默认配置
simple-relay-server

# 自定义配置
simple-relay-server --host 127.0.0.1 --port 9700

# 通过nginx暴露
simple-relay-server --host 127.0.0.1 --port 9700
# nginx stream: 9443 -> 127.0.0.1:9700
```

## 通用选项

### 日志选项

| 选项 | 环境变量 | 默认值 | 说明 |
|------|----------|--------|------|
| `--log-level <LEVEL>` | `LOG_LEVEL` | `info` | 日志级别 |
| `--log-file <PATH>` | `LOG_FILE` | - | 日志文件路径 |

**日志级别：** `trace`, `debug`, `info`, `warn`, `error`, `fatal`

### Admin选项

| 选项 | 环境变量 | 默认值 | 说明 |
|------|----------|--------|------|
| `--admin-enabled` | `ADMIN_ENABLED` | `true` | 启用Admin HTTP |
| `--admin-host <ADDR>` | `ADMIN_HOST` | `127.0.0.1` | Admin监听地址 |
| `--admin-port <PORT>` | `ADMIN_PORT` | `9300` | Admin监听端口 |

### 后台运行

```bash
# 使用nohup
nohup p2p-signaling-server --config /etc/peerlink/peerlink.toml &

# 使用screen
screen -dmS signaling p2p-signaling-server

# 使用systemd（推荐）
sudo systemctl start peerlink-signaling
```

## 常用命令组合

### 完整服务启动

```bash
# 1. 启动Redis
sudo systemctl start redis-server

# 2. 启动Signaling
p2p-signaling-server --config /etc/peerlink/peerlink.toml &

# 3. 启动DID
p2p-did-server --config /etc/peerlink/peerlink.toml &

# 4. 启动Relay
p2p-relay-server --config /etc/peerlink/peerlink.toml &

# 5. 启动Simple Relay
simple-relay-server --host 127.0.0.1 --port 9700 &
```

### relay-tunnel完整流程

```bash
# 办公机端
relay-tunnel server \
    --did office-001 \
    --relay <YOUR_SERVER_IP>:9443 \
    --forward 127.0.0.1:22

# 家里端
relay-tunnel client \
    --did home-mac \
    --target office-001 \
    --relay <YOUR_SERVER_IP>:9443 \
    --listen 9022

# SSH连接
ssh -p 9022 <USER>@127.0.0.1
```

## 故障排查

### 调试模式

```bash
# 启用详细日志
RUST_LOG=debug p2p-signaling-server

# 或使用--log-level
p2p-signaling-server --log-level debug
```

### 检查配置

```bash
# 验证配置文件
p2p-signaling-server --config /etc/peerlink/peerlink.toml --check-config

# 显示当前配置
p2p-signaling-server --config /etc/peerlink/peerlink.toml --show-config
```

### 测试连接

```bash
# 测试relay服务器
printf 'REGISTER test\n' | nc -w 2 <YOUR_SERVER_IP> 9443

# 测试STUN
echo "PING" | nc -u <YOUR_SERVER_IP> 3478
```
