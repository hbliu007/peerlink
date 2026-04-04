---
title: 常见问题
description: PeerLink使用中的常见问题和解答
tags:
  - faq
  - troubleshooting
  - help
---

# 常见问题

本页面收集了PeerLink使用中的常见问题和解答。

## 连接问题

### 办公机STUN检测返回BLOCKED怎么办？

**症状：** STUN检测返回BLOCKED状态

**原因：** 企业防火墙阻止UDP流量

**解决方案：**

1. 直接使用TCP Relay（Layer 3）：
```bash
relay-tunnel server --did office-001 --relay <YOUR_SERVER_IP>:9443 --forward 127.0.0.1:22 --relay-mode relay-only
relay-tunnel client --did home-mac --target office-001 --relay <YOUR_SERVER_IP>:9443 --listen 9022
```

2. 检查企业HTTP代理：
```bash
echo $HTTP_PROXY
echo $HTTPS_PROXY
```

3. 如果必须使用代理，配置代理设置

---

### 如何判断走的是直连还是中继？

**方法1：** 检查连接状态

```cpp
auto state = p2p_client_get_state(client);
if (state == P2P_STATE_CONNECTED_P2P) {
    printf("Direct P2P connection\n");
} else if (state == P2P_STATE_CONNECTED_RELAY) {
    printf("Relayed connection\n");
}
```

**方法2：** 查看日志
```
[INFO] Connection established: P2P (direct)
[INFO] Connection established: RELAY (via <YOUR_SERVER_IP>)
```

**方法3：** 检查延迟
```bash
# P2P连接：~5-20ms
# Relay连接：~30-100ms
ping <peer-ip>
```

---

### 支持多少并发连接？

**服务器端：**
- Gateway: 10,000（默认），可配置到50,000+
- Network: 1,000个TURN分配（默认），可配置
- Signaling: 10,000（默认），可配置到50,000+

**客户端：**
- 默认最大连接数：10
- 可通过配置调整：
```toml
[client]
max_connections = 100
```

---

## 配置问题

### 阿里云安全组怎么配？

**必需端口：**

| 端口 | 协议 | 说明 |
|------|------|------|
| 443 | TCP | HTTPS/WSS入口 |
| 9443 | TCP | TCP Relay（relay-tunnel） |
| 3478 | UDP | STUN服务 |
| 3479 | TCP | STUN over TCP |
| 9001 | UDP | TURN控制 |
| 50000-50100 | UDP | TURN中继数据 |

**配置步骤：**
1. 登录阿里云控制台
2. ECS → 实例 → 安全组
3. 入方向规则 → 添加规则
4. 端口范围按上表配置，授权对象 `0.0.0.0/0`

**注意：** Admin端口（9300-9303）不要对公网开放

---

### JWT_SECRET必须替换吗？

**是的，必须替换！**

**风险：** 使用默认密钥会导致任何人都可以伪造认证令牌

**生成新密钥：**
```bash
openssl rand -base64 32
```

**配置：**
```toml
[auth]
jwt_secret = "your-generated-secret-here"
```

---

### allow_insecure_registration应该设置什么？

**生产环境：** `false`（强制JWT认证）

**开发环境：** `true`（可以临时启用测试）

```toml
[signaling]
allow_insecure_registration = false  # 生产环境
```

---

## 客户端问题

### 手机怎么连？

**方法1：** 通过USB连接笔记本

```bash
# 笔记本上运行tunnel客户端
relay-tunnel client --did home-mac --target office-001 --relay <YOUR_SERVER_IP>:9443 --listen 9022

# Android ADB端口转发
adb forward tcp:9022 tcp:9022

# 手机Termux中
ssh -p 9022 <USER>@127.0.0.1
```

**方法2：** 直连笔记本IP

```bash
# 笔记本监听所有接口
relay-tunnel client --did home-mac --target office-001 --relay <YOUR_SERVER_IP>:9443 --listen 9022 --listen-addr 0.0.0.0

# 手机上（假设笔记本IP是192.168.1.100）
ssh -p 9022 <USER>@192.168.1.100
```

---

### 延迟多少？带宽多少？

| 连接类型 | 延迟 | 带宽 | 适用场景 |
|---------|------|------|----------|
| P2P直连 | 5-20ms | >100Mbps | 实时交互 |
| TCP Relay | 30-100ms | 10-50Mbps | 一般办公 |
| WSS Relay | 50-150ms | 5-20Mbps | 限制环境 |

**实际测量：**
```bash
# 测试延迟
ping -c 10 <peer-ip>

# 测试带宽
iperf3 -c <peer-ip>
```

---

## 性能问题

### 连接建立需要多久？

**P2P直连：** 1-3秒
- STUN检测：~500ms
- 地址交换：~200ms
- 打洞：~500ms-2s

**Relay连接：** 500ms-1s
- 注册到Relay：~200ms
- 连接对端：~300ms

---

### 内存占用多少？

**客户端：**
- 基础内存：~20MB
- 每个连接：~1MB

**服务器：**
- Gateway: ~100MB基础 + 1MB/连接
- Network: ~50MB基础 + 500KB/分配

---

### CPU占用多少？

**空闲时：** <1% CPU

**负载时：**
- 转发1Gbps: ~20% CPU（单核）
- 1000个连接: ~10-15% CPU

---

## 部署问题

### 为什么不用Docker？

PeerLink的官方部署模式是原生Linux服务，原因：

1. **性能：** 原生性能更好，网络延迟更低
2. **调试：** 直接访问系统资源，便于调试
3. **兼容性：** 避免Docker网络复杂性
4. **运维：** 使用systemd管理更符合传统

如果必须使用Docker，社区有非官方的Dockerfile。

---

### 如何升级部署？

```bash
# 1. 备份当前版本
sudo cp /opt/peerlink/bin/peerlink-gateway /opt/peerlink/bin/peerlink-gateway.bak

# 2. 编译新版本
cmake --build build --target peerlink-gateway -j"$(nproc)"

# 3. 停止服务
sudo systemctl stop peerlink-gateway

# 4. 安装新版本
sudo install -m 0755 build/src/servers/gateway/peerlink-gateway /opt/peerlink/bin/

# 5. 启动服务
sudo systemctl start peerlink-gateway

# 6. 验证
sudo systemctl status peerlink-gateway
```

---

### Redis挂了怎么办？

**影响：** Signaling和DID服务无法正常工作

**解决方案：**

1. 检查Redis状态：
```bash
sudo systemctl status redis-server
```

2. 重启Redis：
```bash
sudo systemctl restart redis-server
```

3. 配置Redis持久化：
```conf
# /etc/redis/redis.conf
appendonly yes
appendfsync everysec
```

4. 考虑Redis高可用（哨兵或集群）

---

## 安全问题

### TLS证书过期怎么办？

**检查过期时间：**
```bash
openssl x509 -in /etc/letsencrypt/live/example.com/fullchain.pem -noout -dates
```

**手动续期：**
```bash
sudo certbot renew --force-renewal
sudo systemctl reload peerlink-gateway
```

**自动续期：** 通过cron或systemd timer自动执行

---

### Admin端口被扫描怎么办？

**措施：**

1. 保持Admin端口绑定127.0.0.1：
```toml
[gateway.admin]
host = "127.0.0.1"
```

2. 通过SSH隧道访问：
```bash
ssh -L 9300:localhost:9300 user@server
```

3. 配置防火墙规则：
```bash
sudo ufw allow from 192.168.1.0/24 to any port 9300
```

---

### 如何防止DDoS攻击？

**内置保护：**

1. **限速：** Token Bucket算法
```toml
[rate_limit]
requests_per_second = 10
burst_size = 20
```

2. **自动封禁：** 违规阈值后自动封禁
```toml
[rate_limit]
ban_threshold = 5
ban_duration_seconds = 300
```

3. **连接限制：** 最大连接数限制

**额外措施：**
- 使用CDN
- 配置防火墙规则
- 启用Fail2Ban

---

## 兼容性问题

### 和WireGuard/Tailscale什么关系？

**WireGuard：** VPN内核模块
- 优点：高性能、低延迟
- 缺点：需要内核支持、配置复杂

**Tailscale：** 基于WireGuard的零配置VPN
- 优点：易用、NAT穿透好
- 缺点：依赖第三方服务

**PeerLink：** 应用层P2P SDK
- 优点：嵌入应用、三层降级
- 缺点：性能略低于WireGuard

**关系：** 可以互补使用
- PeerLink处理应用层连接
- WireGuard/Tailscale处理网络层

---

### 支持哪些平台？

**服务端：**
- Linux: Ubuntu 20.04+, Debian 11+, CentOS 8+
- macOS: 11+

**客户端：**
- C/C++: Linux, macOS, Windows
- Python: 3.8+
- Java: 11+
- JavaScript: Node 16+, Browser

---

### 支持IPv6吗？

**部分支持：**

- STUN/TURN: 支持IPv6
- Signaling: 支持IPv6
- Simple Relay: 暂不支持

**配置IPv6：**
```toml
[stun]
host = "::"
udp_port = 3478
```

---

## 故障排查

### 连接失败怎么办？

**诊断步骤：**

1. **检查网络连通性：**
```bash
ping <server-ip>
telnet <server-ip> <port>
```

2. **检查防火墙：**
```bash
sudo ufw status
sudo iptables -L -n
```

3. **检查服务状态：**
```bash
sudo systemctl status peerlink-gateway
sudo journalctl -u peerlink-gateway -n 50
```

4. **检查配置：**
```bash
cat /etc/peerlink/peerlink.toml
```

5. **启用调试日志：**
```toml
[logging]
level = "debug"
```

---

### 性能下降怎么办？

**检查项：**

1. **CPU使用率：**
```bash
top -p $(pgrep peerlink-gateway)
```

2. **内存使用：**
```bash
ps aux | grep peerlink
```

3. **网络带宽：**
```bash
iftop -i eth0
```

4. **连接数：**
```bash
curl http://127.0.0.1:9300/status | jq '.connections'
```

---

### 如何收集调试信息？

**完整诊断脚本：**
```bash
#!/bin/bash
echo "=== PeerLink Diagnostics ==="
echo ""
echo "Service Status:"
systemctl status peerlink-gateway --no-pager
echo ""
echo "Recent Logs:"
journalctl -u peerlink-gateway -n 50 --no-pager
echo ""
echo "Network Stats:"
ss -s
echo ""
echo "Listening Ports:"
ss -lntup | grep peerlink
echo ""
echo "Config File:"
cat /etc/peerlink/peerlink.toml
echo ""
echo "System Info:"
uname -a
free -h
df -h
```

运行：
```bash
bash /usr/local/bin/peerlink-diagnose > diagnostics.txt
```
