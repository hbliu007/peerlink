---
title: 防火墙穿透配置指南
description: 配置三层降级架构实现企业防火墙穿透，包括STUN、UDP打洞、TCP Relay和WSS封装
tags:
  - nat
  - firewall
  - stun
  - relay
  - penetration
---

# 防火墙穿透配置指南

本指南介绍如何配置PeerLink的三层降级架构，实现企业防火墙穿透。

## 问题背景

办公机位于企业防火墙后时：
- STUN检测返回BLOCKED
- 无法直连云服务器的任意端口
- 目标：从家庭设备远程连接办公机

## 三层降级架构

```
┌──────────────────────────────────────────────┐
│ Layer 1: UDP Hole Punching (最低延迟)         │
│   - STUN检测NAT类型                           │
│   - 通过信令交换公网地址                       │
│   - 双方同时发送UDP包打洞                      │
│   - 成功率: Cone NAT ~80%, Symmetric NAT ~5%  │
├──────────────────────────────────────────────┤
│ Layer 2: TCP Hole Punching (中等延迟)         │
│   - TCP Simultaneous Open                     │
│   - 双方同时SYN                               │
│   - 成功率: ~30%                              │
├──────────────────────────────────────────────┤
│ Layer 3: TCP 443 TLS Relay (保底, ~100%成功)  │
│   - 双端出站连接 Relay Server:443             │
│   - Relay双向转发                             │
│   - 流量特征与HTTPS一致                       │
│   - 可选WSS封装+HTTP CONNECT代理支持          │
└──────────────────────────────────────────────┘
```

## Layer 1: UDP打洞配置

### STUN服务器配置

编辑配置文件：

```toml
[stun]
host = "0.0.0.0"
udp_port = 3478
tcp_port = 3479
```

### 防火墙规则

开放以下端口：

| 端口 | 协议 | 用途 |
|------|------|------|
| 3478 | UDP | STUN服务 |
| 3479 | TCP | STUN over TCP |

### NAT类型检测

客户端会自动进行STUN检测：

```cpp
// 客户端代码示例
auto stun_result = client->DetectNATType();
switch (stun_result.type) {
    case NATType::OPEN:
        // 公网，无需打洞
        break;
    case NATType::CONE:
        // Cone NAT，UDP打洞成功率~80%
        break;
    case NATType::SYMMETRIC:
        // 对称NAT，UDP打洞成功率~5%
        // 降级到Layer 2/3
        break;
    case NATType::BLOCKED:
        // 防火墙阻止，直接使用Layer 3
        break;
}
```

### UDP打洞流程

```
1. 双方连接STUN服务器获取公网地址
2. 通过信令服务器交换地址信息
3. 双方同时向对方公网地址发送UDP包
4. NAT创建映射，打洞成功
5. 建立直连P2P连接
```

## Layer 2: TCP打洞配置

### TCP Simultaneous Open

```toml
[signaling]
enable_tcp_hole_punching = true
tcp_hole_punch_timeout = 30  # 秒
```

### TCP打洞流程

```
1. 双方通过信令协商TCP端口
2. 双方同时向对方公网地址发送SYN包
3. NAT创建映射，TCP连接建立
4. 成功后升级为P2P连接
```

!!! note "成功率说明"
    TCP打洞成功率约30%，取决于NAT类型。大多数企业NAT会阻止TCP打洞。

## Layer 3: TCP Relay配置

### TURN/Relay服务器配置

```toml
[turn]
host = "0.0.0.0"
port = 9001
public_ip = "<YOUR_SERVER_IP>"  # 替换为你的公网IP
min_port = 50000
max_port = 50100
lifetime = 600
max_allocations = 1000
```

### Simple Relay配置（用于relay-tunnel）

```bash
# 启动simple-relay-server
/opt/peerlink/bin/simple-relay-server --host 127.0.0.1 --port 9700
```

通过nginx暴露：

```nginx
# /etc/nginx/peerlink-stream.conf
stream {
    upstream peerlink_simple_relay {
        server 127.0.0.1:9700;
    }

    server {
        listen 9443;
        listen [::]:9443;
        proxy_pass peerlink_simple_relay;
        proxy_timeout 7d;
        proxy_connect_timeout 60s;
    }
}
```

### 客户端配置

```toml
[relay]
enabled = true
server = "<YOUR_SERVER_IP>:9443"
mode = "relay-only"  # 强制使用relay
```

### relay-tunnel使用

**办公机端（tunnel server）：**

```bash
./relay-tunnel server \
    --did office-001 \
    --relay <YOUR_SERVER_IP>:9443 \
    --forward 127.0.0.1:22
```

**笔记本端（tunnel client）：**

```bash
./relay-tunnel client \
    --did home-001 \
    --target office-001 \
    --relay <YOUR_SERVER_IP>:9443 \
    --listen 9022
```

**连接SSH：**

```bash
ssh -p 9022 <USER>@127.0.0.1
```

## WSS封装（DPI对抗）

### 启用WSS

```toml
[signaling.tls]
enabled = true
cert_path = "/etc/letsencrypt/live/your-domain.com/fullchain.pem"
key_path = "/etc/letsencrypt/live/your-domain.com/privkey.pem"
```

### Nginx WebSocket代理

```nginx
server {
    listen 443 ssl http2;
    server_name your-domain.com;

    ssl_certificate /etc/letsencrypt/live/your-domain.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/your-domain.com/privkey.pem;

    location / {
        proxy_pass http://127.0.0.1:18080;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host $host;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto https;
    }
}
```

!!! tip "DPI对抗"
    WSS封装使流量完全伪装成HTTPS，有效绕过深度包检测（DPI）。

## HTTP CONNECT代理穿透

### 代理配置

当企业强制使用HTTP代理时：

```bash
# 设置代理环境变量
export HTTP_PROXY=http://proxy.company.com:8080
export HTTPS_PROXY=http://proxy.company.com:8080
```

### 客户端配置

```cpp
// 客户端代码示例
auto config = p2p_config_create();
p2p_config_set_proxy(config, "http://proxy.company.com:8080");
```

## 端口配置总结

### 云服务器端口

| 端口 | 协议 | 层级 | 用途 |
|------|------|------|------|
| 443 | TCP | Layer 3 | HTTPS/WSS入口 |
| 9443 | TCP | Layer 3 | TCP Relay |
| 3478 | UDP | Layer 1 | STUN服务 |
| 3479 | TCP | Layer 2 | STUN over TCP |
| 9001 | UDP | Layer 3 | TURN控制 |
| 50000-50100 | UDP | Layer 3 | TURN中继数据 |

### 客户端配置

```toml
[client]
# STUN服务器
stun_server = "<YOUR_SERVER_IP>:3478"

# TURN/Relay服务器
relay_server = "<YOUR_SERVER_IP>:9443"

# 信令服务器
signaling_server = "wss://your-domain.com"

# 连接模式
connection_mode = "auto"  # auto, p2p-only, relay-only
```

## 验证穿透效果

### 1. STUN测试

```bash
# 从办公机测试
echo "PING" | nc -u <YOUR_SERVER_IP> 3478
```

### 2. Relay测试

```bash
# 测试TCP Relay
printf 'REGISTER test\n' | nc -w 2 <YOUR_SERVER_IP> 9443
```

### 3. WSS测试

```bash
# 安装wscat
npm install -g wscat

# 测试WSS连接
wscat -c wss://your-domain.com
```

### 4. 端到端测试

```bash
# 从办公机
./relay-tunnel server --did office-001 --relay <YOUR_SERVER_IP>:9443 --forward 127.0.0.1:22

# 从家里
./relay-tunnel client --did home-001 --target office-001 --relay <YOUR_SERVER_IP>:9443 --listen 9022

# 测试SSH
ssh -p 9022 <USER>@127.0.0.1
```

## 性能预期

| 指标 | Layer 1 (UDP) | Layer 2 (TCP) | Layer 3 (Relay) |
|------|--------------|--------------|----------------|
| 延迟 | ~5-20ms | ~20-50ms | ~30-100ms |
| 带宽 | >100Mbps | >50Mbps | ~10-50Mbps |
| 成功率 | 80% | 30% | ~100% |

## 常见问题

### Q: STUN检测返回BLOCKED怎么办？

A: 说明企业防火墙完全阻止UDP。直接使用Layer 3（TCP Relay）。

### Q: UDP打洞失败但TCP打洞成功？

A: 正常现象。不同NAT类型对UDP和TCP的处理不同。

### Q: 如何判断走的是直连还是中继？

A: 检查连接状态：

```cpp
auto state = p2p_client_get_state(client);
if (state == P2P_STATE_CONNECTED_P2P) {
    // 直连
} else if (state == P2P_STATE_CONNECTED_RELAY) {
    // 中继
}
```

### Q: 手机怎么连？

A: 手机通过USB连接笔记本，使用笔记本的隧道：

```bash
# Termux中
ssh -p 9022 <USER>@<笔记本IP>
```

## 最佳实践

1. **优先使用Layer 3**：企业环境下直接用TCP Relay最稳定
2. **WSS封装**：生产环境建议启用，对抗DPI
3. **自动降级**：客户端配置`connection_mode = "auto"`自动选择最佳方案
4. **监控告警**：监控连接类型分布，及时发现网络问题
