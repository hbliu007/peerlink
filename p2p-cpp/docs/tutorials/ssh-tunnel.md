---
title: SSH 隧道教程
description: 使用 relay-tunnel 工具从家庭电脑远程 SSH 访问企业防火墙后的办公机
tags:
  - 教程
  - SSH 隧道
  - 远程访问
  - relay-tunnel
---

# SSH 隧道教程

本教程展示如何使用 PeerLink 的 `relay-tunnel` 工具，从家庭电脑通过 Relay 中转 SSH 访问位于企业防火墙后的办公机。

## 应用场景

```
家庭笔记本 ──TCP 443──→ 阿里云 Relay Server ←──TCP 443── 办公机（防火墙后）
    │                        │                          │
    │  ssh -p 9022           │   数据双向转发             │  SSH 服务 (22 端口)
    │  <USER>@127.0.0.1         │   流量特征同 HTTPS        │
    └────────────────────────┴──────────────────────────┘
```

!!! tip "为什么需要 Relay?"
    企业防火墙通常封锁所有入站连接，但几乎 100% 放行出站 TCP 443（HTTPS）。Relay Server 利用这一特性，让办公机主动出站连接到云端，再由家庭电脑连接同一台 Relay，实现双向数据通道。

---

## 前提条件

| 条件 | 说明 |
|------|------|
| 阿里云服务器 | 已部署 Relay Server（如 `<YOUR_SERVER_IP>:9443`） |
| relay-tunnel 二进制 | 办公机和笔记本上各一份 |
| 防火墙出站 443 | 办公机能出站连接云端 443 端口 |
| SSH 服务 | 办公机上运行 `sshd`（默认端口 22） |

!!! warning "阿里云安全组"
    确保阿里云安全组入方向规则放行 TCP 9443 端口。如需使用 443 端口，需同时放行 TCP 443。

---

## 步骤 1: 准备 relay-tunnel 二进制

### 从源码编译

```bash
# macOS
g++ -std=c++17 -O2 -I/opt/homebrew/include \
  -DBOOST_ASIO_NO_DEPRECATED \
  -o relay-tunnel tools/p2p-tunnel/relay_tunnel.cpp \
  -lpthread

# Linux
g++ -std=c++17 -O2 \
  -o relay-tunnel tools/p2p-tunnel/relay_tunnel.cpp \
  -lpthread -lboost_system
```

### 传输到办公机

```bash
# 通过跳板机传输
scp relay-tunnel <USER>@office-host:/home/<USER>/relay-tunnel
chmod +x /home/<USER>/relay-tunnel
```

---

## 步骤 2: 办公机端启动 Tunnel Server

在办公机上执行：

```bash
# 多会话模式（推荐，支持多个并发 SSH）
./relay-tunnel server \
  --did office-213 \
  --relay <YOUR_SERVER_IP>:9443 \
  --forward 127.0.0.1:22
```

参数说明：

| 参数 | 说明 | 示例 |
|------|------|------|
| `--did` | 设备标识（自定义，需唯一） | `office-213` |
| `--relay` | Relay Server 地址 | `<YOUR_SERVER_IP>:9443` |
| `--forward` | 转发到本地服务的地址 | `127.0.0.1:22` |

启动成功后会看到：

```
=== Relay Tunnel Server ===
DID: office-213
Relay: <YOUR_SERVER_IP>:9443
Forward: 127.0.0.1:22
[Server] Starting multi-session server...
[Server] Session #1 - Registered, waiting for client connection...
```

!!! tip "后台运行"
    使用 `nohup` 或 `tmux` 让 tunnel 在后台持续运行：
    ```bash
    nohup ./relay-tunnel server --did office-213 \
      --relay <YOUR_SERVER_IP>:9443 --forward 127.0.0.1:22 \
      > /tmp/relay.log 2>&1 &
    ```

---

## 步骤 3: 笔记本端启动 Tunnel Client

在家庭笔记本上执行：

```bash
# 多会话模式
./relay-tunnel client \
  --did home-mac \
  --target office-213 \
  --relay <YOUR_SERVER_IP>:9443 \
  --listen 9022
```

参数说明：

| 参数 | 说明 | 示例 |
|------|------|------|
| `--did` | 本设备标识 | `home-mac` |
| `--target` | 目标设备的 DID（办公机） | `office-213` |
| `--relay` | Relay Server 地址 | `<YOUR_SERVER_IP>:9443` |
| `--listen` | 本地监听端口 | `9022` |

启动成功后会看到：

```
=== Relay Tunnel Client ===
DID: home-mac
Target: office-213
Relay: <YOUR_SERVER_IP>:9443
Listen: 127.0.0.1:9022
[Client] Tunnel ready! Use: ssh -p 9022 <USER>@127.0.0.1
```

---

## 步骤 4: 测试 SSH 连接

```bash
# 基本连接测试
ssh -p 9022 -o PubkeyAuthentication=no <USER>@127.0.0.1

# 一行命令验证
ssh -p 9022 -o PubkeyAuthentication=no <USER>@127.0.0.1 'hostname; whoami; echo SUCCESS'
```

预期输出：

```
office-server
lhb
SUCCESS
```

!!! tip "禁用公钥认证"
    首次连接时必须加 `-o PubkeyAuthentication=no`，否则 SSH 可能会因密钥交换卡住。

### 多会话并发

使用多会话版本（`relay_tunnel.cpp`）时，可以同时开多个终端：

```bash
ssh -p 9022 -o PubkeyAuthentication=no <USER>@127.0.0.1  # 终端 1
ssh -p 9022 -o PubkeyAuthentication=no <USER>@127.0.0.1  # 终端 2
ssh -p 9022 -o PubkeyAuthentication=no <USER>@127.0.0.1  # 终端 3
```

---

## 手机连接方式

### 方式 1: ADB 端口转发（USB 连接）

```bash
# 笔记本上执行（手机通过 USB 连接）
adb forward tcp:9022 tcp:9022

# 手机 Termux 中
ssh -p 9022 -o PubkeyAuthentication=no <USER>@127.0.0.1
```

### 方式 2: WiFi 直连

```bash
# 笔记本上修改监听地址为所有接口
./relay-tunnel client \
  --did home-mac \
  --target office-213 \
  --relay <YOUR_SERVER_IP>:9443 \
  --listen 0.0.0.0:9022

# 手机上（假设笔记本 IP 是 192.168.1.100）
ssh -p 9022 -o PubkeyAuthentication=no <USER>@192.168.1.100
```

!!! warning "WiFi 直连安全提示"
    `0.0.0.0` 监听会让同一 WiFi 下的所有设备都能访问。建议只在可信网络中使用，或通过防火墙限制来源 IP。

详细手机配置请参考 [手机连接教程](mobile-connect.md)。

---

## 故障排查

### 问题 1: 连接超时

```bash
# 检查 tunnel 进程是否运行
ps aux | grep relay-tunnel

# 检查本地端口是否监听
netstat -tlnp | grep 9022

# 测试 Relay Server 连通性
echo "PING" | nc -w3 <YOUR_SERVER_IP> 9443
```

### 问题 2: SSH 连接卡住

- 确认添加了 `-o PubkeyAuthentication=no`
- 检查办公机 sshd 是否运行：`systemctl status sshd`
- 查看 tunnel 日志是否有错误信息

### 问题 3: Tunnel 频繁断开

```bash
# 查看 relay 日志
tail -f /tmp/relay.log

# 检查网络稳定性
ping -c 10 <YOUR_SERVER_IP>
```

### 问题 4: 办公机端 tunnel 未运行

通过跳板机远程重启：

```bash
sshpass -p '<PASSWORD>' ssh -J root@jump-host:port <USER>@<OFFICE_SERVER_1> \
  'nohup /home/<USER>/relay-tunnel server --did office-213 \
   --relay <YOUR_SERVER_IP>:9443 --forward 127.0.0.1:22 \
   > /tmp/relay.log 2>&1 &'
```

---

## 性能参考

| 指标 | 直连 SSH | Relay 中转 SSH |
|------|---------|---------------|
| 延迟 | ~5 ms | ~30-50 ms |
| 带宽 | >100 Mbps | ~10-50 Mbps |
| 适用场景 | 正常操作 | Claude Code CLI 等终端操作 |

终端操作对延迟不敏感，50 ms 延迟完全够用。

---

## 下一步

- [手机连接教程](mobile-connect.md) - 从 Android/iPhone 连接办公机
- [Python SDK 快速入门](python-sdk.md) - 用代码建立 P2P 通道
- [防火墙穿透设计](../concepts/architecture.md) - 了解底层原理
