---
title: 安全加固指南
description: 生产环境安全加固最佳实践，包括认证、加密、访问控制和密钥管理
tags:
  - security
  - hardening
  - jwt
  - tls
  - access-control
---

# 安全加固指南

本指南介绍PeerLink生产环境的安全加固最佳实践。

## 核心安全原则

1. **最小权限原则**：每个组件只授予必要的最小权限
2. **纵深防御**：多层安全控制
3. **默认拒绝**：默认拒绝所有访问，显式允许需要的访问
4. **加密传输**：所有网络通信必须加密
5. **定期审计**：定期检查和更新安全配置

## JWT密钥配置

### 生成强随机密钥

```bash
# 生成256位随机密钥
openssl rand -base64 32

# 或使用uuidgen（128位）
uuidgen | tr -d '-'
```

### 配置JWT密钥

编辑 `/etc/peerlink/peerlink.toml`：

```toml
[auth]
jwt_secret = "your-generated-secret-key-here"
```

### 环境变量配置

```bash
# 更安全的方式：通过环境变量
export JWT_SECRET="$(openssl rand -base64 32)"
```

!!! danger "安全警告"
    - **永远不要**在生产环境使用示例配置中的密钥
    - 密钥长度至少256位（32字节）
    - 定期轮换密钥（建议每6个月）

## 认证配置

### 强制认证注册

```toml
[signaling]
# 保持为false，强制使用JWT认证
allow_insecure_registration = false
```

### JWT令牌生成

```python
import jwt
import time

# 生成JWT令牌
payload = {
    'sub': 'device-001',  # 设备ID
    'iat': int(time.time()),
    'exp': int(time.time()) + 3600  # 1小时过期
}

token = jwt.encode(payload, 'your-secret', algorithm='HS256')
print(token)
```

### 客户端使用JWT

```cpp
// 设置认证令牌
config->SetAuthToken("your-jwt-token");
```

## Admin端口保护

### 绑定本地回环

```toml
[gateway.admin]
enabled = true
host = "127.0.0.1"  # 仅监听本地
port = 9300

[network.admin]
enabled = true
host = "127.0.0.1"
port = 9301

[signaling.admin]
enabled = true
host = "127.0.0.1"
port = 9302

[did.admin]
enabled = true
host = "127.0.0.1"
port = 9303
```

### 通过SSH隧道访问

```bash
# 从远程访问管理接口
ssh -L 9300:localhost:9300 user@server

# 然后在本地访问
curl http://localhost:9300/healthz
```

### Nginx反向代理认证

```nginx
server {
    listen 8080;
    server_name admin.your-domain.com;

    # 基本认证
    auth_basic "Admin Area";
    auth_basic_user_file /etc/nginx/.htpasswd;

    location / {
        proxy_pass http://127.0.0.1:9300;
    }
}
```

创建密码文件：

```bash
sudo htpasswd -c /etc/nginx/.htpasswd admin
```

## TLS配置

### 强制启用TLS

```toml
[signaling.tls]
enabled = true
cert_path = "/etc/letsencrypt/live/your-domain.com/fullchain.pem"
key_path = "/etc/letsencrypt/live/your-domain.com/privkey.pem"

[did.tls]
enabled = true
cert_path = "/etc/letsencrypt/live/your-domain.com/fullchain.pem"
key_path = "/etc/letsencrypt/live/your-domain.com/privkey.pem"
```

### TLS 1.3强制

```toml
[signaling.tls]
min_tls_version = "1.3"
max_tls_version = "1.3"
```

### 密码套件配置

```toml
[signaling.tls]
cipher_suites = "TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256"
```

## Ed25519密钥管理

### 生成Ed25519密钥对

```bash
# 使用ssh-keygen
ssh-keygen -t ed25519 -f peerlink_ed25519 -C "peerlink"

# 或使用openssl
openssl genpkey -algorithm Ed25519 -out peerlink_private.pem
openssl pkey -in peerlink_private.pem -pubout -out peerlink_public.pem
```

### 配置Ed25519密钥

```toml
[p2p]
identity_key_path = "/etc/peerlink/ed25519_private.pem"
peer_key_path = "/etc/peerlink/ed25519_public.pem"
```

### 设置权限

```bash
# 私钥必须严格保护
sudo chmod 600 /etc/peerlink/ed25519_private.pem
sudo chown peerlink:peerlink /etc/peerlink/ed25519_private.pem

# 公钥可读
sudo chmod 644 /etc/peerlink/ed25519_public.pem
```

## 防火墙配置

### UFW (Ubuntu)

```bash
# 默认拒绝
sudo ufw default deny incoming
sudo ufw default allow outgoing

# 允许SSH
sudo ufw allow 22/tcp

# 允许HTTPS/WSS
sudo ufw allow 443/tcp

# 允许特定IP访问管理端口
sudo ufw allow from 192.168.1.0/24 to any port 9300
sudo ufw allow from 192.168.1.0/24 to any port 9301

# 允许STUN/TURN
sudo ufw allow 3478/udp
sudo ufw allow 3479/tcp
sudo ufw allow 9001/udp
sudo ufw allow 50000:50100/udp

# 启用防火墙
sudo ufw enable
```

### firewalld (CentOS/RHEL)

```bash
# 默认区域
sudo firewall-cmd --set-default-zone=public

# 允许服务
sudo firewall-cmd --permanent --add-service=ssh
sudo firewall-cmd --permanent --add-service=https

# 允许端口
sudo firewall-cmd --permanent --add-port=443/tcp
sudo firewall-cmd --permanent --add-port=3478/udp
sudo firewall-cmd --permanent --add-port=9001/udp
sudo firewall-cmd --permanent --add-port=50000-50100/udp

# 重载
sudo firewall-cmd --reload
```

### iptables

```bash
# 清空规则
sudo iptables -F
sudo iptables -X

# 默认策略
sudo iptables -P INPUT DROP
sudo iptables -P FORWARD DROP
sudo iptables -P OUTPUT ACCEPT

# 允许本地回环
sudo iptables -A INPUT -i lo -j ACCEPT

# 允许已建立的连接
sudo iptables -A INPUT -m state --state ESTABLISHED,RELATED -j ACCEPT

# 允许SSH
sudo iptables -A INPUT -p tcp --dport 22 -j ACCEPT

# 允许HTTPS
sudo iptables -A INPUT -p tcp --dport 443 -j ACCEPT

# 允许STUN/TURN
sudo iptables -A INPUT -p udp --dport 3478 -j ACCEPT
sudo iptables -A INPUT -p udp --dport 9001 -j ACCEPT
sudo iptables -A INPUT -p udp --dport 50000:50100 -j ACCEPT

# 保存规则
sudo iptables-save | sudo tee /etc/iptables/rules.v4
```

## Redis安全

### 绑定本地地址

```toml
[redis]
host = "127.0.0.1"  # 不要绑定0.0.0.0
port = 6379
```

### 配置Redis认证

编辑 `/etc/redis/redis.conf`：

```conf
# 绑定本地
bind 127.0.0.1

# 设置密码
requirepass your-redis-password

# 禁用危险命令
rename-command CONFIG ""
rename-command FLUSHDB ""
rename-command FLUSHALL ""
```

### 配置文件权限

```bash
sudo chmod 600 /etc/redis/redis.conf
sudo chown redis:redis /etc/redis/redis.conf
```

## 系统安全

### 文件描述符限制

```bash
# 编辑/etc/security/limits.conf
peerlink soft nofile 65535
peerlink hard nofile 65535
```

### 内核参数优化

编辑 `/etc/sysctl.conf`：

```conf
# 网络安全
net.ipv4.ip_forward = 0
net.ipv4.conf.all.accept_source_route = 0
net.ipv4.conf.default.accept_source_route = 0
net.ipv4.conf.all.accept_redirects = 0
net.ipv4.conf.default.accept_redirects = 0
net.ipv4.conf.all.send_redirects = 0
net.ipv4.conf.default.send_redirects = 0

# SYN保护
net.ipv4.tcp_syncookies = 1
net.ipv4.tcp_max_syn_backlog = 2048
net.ipv4.tcp_synack_retries = 2
net.ipv4.tcp_syn_retries = 5

# 防止SYN攻击
net.ipv4.tcp_syncookies = 1
net.ipv4.tcp_max_syn_backlog = 8192
net.ipv4.tcp_syncookies = 1

# 防止IP欺骗
net.ipv4.conf.all.rp_filter = 1
net.ipv4.conf.default.rp_filter = 1
```

应用配置：

```bash
sudo sysctl -p
```

### 服务隔离

```ini
# systemd服务配置
[Service]
# 使用专用用户
User=peerlink
Group=peerlink

# 限制权限
NoNewPrivileges=true

# 限制资源
LimitNOFILE=65535
MemoryLimit=1G
CPUQuota=200%

# 只读文件系统（如果可能）
# ReadOnlyDirectories=/
```

## 日志安全

### 配置日志级别

```toml
[logging]
level = "info"  # 生产环境使用info或warn
file = "/var/log/peerlink/peerlink.log"
max_size = "100M"
max_files = 10
```

### 日志轮转

创建 `/etc/logrotate.d/peerlink`：

```
/var/log/peerlink/*.log {
    daily
    rotate 14
    compress
    delaycompress
    missingok
    notifempty
    create 0640 peerlink peerlink
    sharedscripts
    postrotate
        systemctl reload peerlink-gateway > /dev/null 2>&1 || true
    endscript
}
```

### 保护敏感日志

```bash
# 设置日志目录权限
sudo chmod 750 /var/log/peerlink
sudo chown peerlink:peerlink /var/log/peerlink

# 确保日志中不包含敏感信息
# 不要记录完整的JWT令牌、密码等
```

## 安全检查清单

部署前检查：

- [ ] JWT_SECRET已替换为强随机密钥
- [ ] allow_insecure_registration设置为false
- [ ] Admin端口仅绑定127.0.0.1
- [ ] TLS已启用并配置有效证书
- [ ] 防火墙规则已正确配置
- [ ] Redis已设置密码
- [ ] 服务使用专用用户运行
- [ ] 文件权限已正确设置
- [ ] 日志配置已优化
- [ ] 内核参数已优化
- [ ] 备份策略已制定
