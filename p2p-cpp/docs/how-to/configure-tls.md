---
title: TLS配置指南
description: 配置TLS 1.3加密通信，包括开发环境自签名证书和生产环境Let's Encrypt证书
tags:
  - tls
  - security
  - nginx
  - certificates
---

# TLS配置指南

本指南介绍如何为PeerLink配置TLS 1.3加密通信，保护所有网络传输安全。

## 架构概览

```
┌─────────────────┐     WSS (TLS 1.3)      ┌──────────────────┐
│  Client Device  │ ◄──────────────────────► │ Signaling Server │
└─────────────────┘                         └──────────────────┘
                                                    :8443

┌─────────────────┐     HTTPS (TLS 1.3)    ┌──────────────────┐
│  Admin Console  │ ◄──────────────────────► │   DID Server     │
└─────────────────┘                         └──────────────────┘
                                                    :8444
```

## 开发环境

### 生成自签名证书

```bash
cd /path/to/p2p-cpp

# 创建证书目录
mkdir -p certs

# 生成自签名证书（有效期365天）
openssl req -x509 -newkey rsa:2048 -nodes \
    -keyout certs/server.key \
    -out certs/server.crt \
    -days 365 \
    -subj "/CN=localhost"
```

这将生成：
- `certs/server.crt` - 服务器证书
- `certs/server.key` - 私钥

### 配置TLS

编辑配置文件 `config/peerlink.toml`：

```toml
[signaling]
host = "0.0.0.0"
port = 8443

[signaling.tls]
enabled = true
cert_path = "certs/server.crt"
key_path = "certs/server.key"
cipher_suites = "TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256:TLS_AES_128_GCM_SHA256"

[did]
host = "0.0.0.0"
port = 8444

[did.tls]
enabled = true
cert_path = "certs/server.crt"
key_path = "certs/server.key"
```

### 启动服务器

```bash
# 使用配置文件启动
export PEERLINK_CONFIG=config/peerlink.toml
./build/src/servers/signaling/p2p-signaling-server
./build/src/servers/did/p2p-did-server
```

### 测试连接

```bash
# 测试TLS握手
openssl s_client -connect localhost:8443 -tls1_3

# 测试WSS连接（需要安装wscat）
npm install -g wscat
wscat -c wss://localhost:8443 --no-check

# 测试HTTPS请求
curl --insecure https://localhost:8444/health
```

## 生产环境

### 安装Certbot

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install certbot

# CentOS/RHEL
sudo yum install certbot

# macOS
brew install certbot
```

### 获取Let's Encrypt证书

#### 方式1：Standalone模式

```bash
# 停止现有服务器
sudo systemctl stop peerlink-gateway

# 获取证书
sudo certbot certonly --standalone -d your-domain.com

# 重启服务器
sudo systemctl start peerlink-gateway
```

#### 方式2：Webroot模式

```bash
sudo certbot certonly --webroot -w /var/www/html -d your-domain.com
```

#### 方式3：DNS验证（适用于内网服务器）

```bash
sudo certbot certonly --manual --preferred-challenges dns -d your-domain.com
```

证书位置：
- 证书链：`/etc/letsencrypt/live/your-domain.com/fullchain.pem`
- 私钥：`/etc/letsencrypt/live/your-domain.com/privkey.pem`

### 配置生产TLS

编辑 `/etc/peerlink/peerlink.toml`：

```toml
[signaling.tls]
enabled = true
cert_path = "/etc/letsencrypt/live/your-domain.com/fullchain.pem"
key_path = "/etc/letsencrypt/live/your-domain.com/privkey.pem"
cipher_suites = "TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256:TLS_AES_128_GCM_SHA256"
ocsp_stapling = true
```

### Nginx TLS终结配置

创建 `/etc/nginx/sites-available/peerlink-gateway.conf`：

```nginx
server {
    listen 443 ssl http2;
    server_name your-domain.com;

    # 证书配置
    ssl_certificate /etc/letsencrypt/live/your-domain.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/your-domain.com/privkey.pem;

    # TLS 1.3 only
    ssl_protocols TLSv1.3;

    # 现代密码套件
    ssl_ciphers TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256:TLS_AES_128_GCM_SHA256;

    # 会话缓存
    ssl_session_cache shared:SSL:10m;
    ssl_session_timeout 10m;

    # OCSP Stapling
    ssl_stapling on;
    ssl_stapling_verify on;
    ssl_trusted_certificate /etc/letsencrypt/live/your-domain.com/chain.pem;

    # HSTS
    add_header Strict-Transport-Security "max-age=31536000" always;

    location / {
        proxy_pass http://127.0.0.1:18080;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host $host;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto https;
        proxy_read_timeout 300s;
    }
}
```

### 自动续期

Let's Encrypt证书有效期90天，需要自动续期。

#### Cron方式

```bash
# 编辑crontab
sudo crontab -e

# 添加每天凌晨2点检查续期
0 2 * * * certbot renew --quiet --deploy-hook "systemctl reload peerlink-gateway peerlink-did peerlink-signaling"
```

#### Systemd Timer方式

创建 `/etc/systemd/system/certbot-renew.timer`：

```ini
[Unit]
Description=Certbot Renewal Timer

[Timer]
OnCalendar=daily
RandomizedDelaySec=1h
Persistent=true

[Install]
WantedBy=timers.target
```

创建 `/etc/systemd/system/certbot-renew.service`：

```ini
[Unit]
Description=Certbot Renewal

[Service]
Type=oneshot
ExecStart=/usr/bin/certbot renew --quiet
ExecStartPost=/bin/systemctl reload peerlink-gateway peerlink-did
```

启用：

```bash
sudo systemctl enable certbot-renew.timer
sudo systemctl start certbot-renew.timer
```

## WSS验证

### 验证WSS连接

```bash
# 安装wscat
npm install -g wscat

# 连接WSS端点
wscat -c wss://your-domain.com --no-check
```

### 验证TLS版本

```bash
# 检查TLS 1.3
openssl s_client -connect your-domain.com:443 -tls1_3

# 检查协商的密码套件
openssl s_client -connect your-domain.com:443 -tls1_3 | grep "Cipher"
```

### 验证证书

```bash
# 查看证书详情
openssl x509 -in /etc/letsencrypt/live/your-domain.com/fullchain.pem -text -noout

# 检查过期时间
openssl x509 -in /etc/letsencrypt/live/your-domain.com/fullchain.pem -noout -dates

# 验证证书链
openssl s_client -connect your-domain.com:443 -showcerts
```

## 故障排查

### SSL握手失败

```bash
# 1. 检查证书有效性
openssl x509 -in certs/server.crt -text -noout

# 2. 检查私钥匹配
CERT_MD5=$(openssl x509 -noout -modulus -in certs/server.crt | openssl md5)
KEY_MD5=$(openssl rsa -noout -modulus -in certs/server.key | openssl md5)
echo "Cert: $CERT_MD5"
echo "Key:  $KEY_MD5"
# 两者MD5应该相同

# 3. 测试SSL连接
openssl s_client -connect localhost:8443 -tls1_3 -showcerts
```

### 证书过期

```bash
# 检查过期时间
openssl x509 -in certs/server.crt -noout -dates

# 手动续期Let's Encrypt证书
sudo certbot renew --force-renewal

# 重新加载服务器
sudo systemctl reload peerlink-gateway
```

### 性能问题

```bash
# 1. 检查AES-NI硬件加速支持
grep -o 'aes' /proc/cpuinfo | wc -l

# 2. 验证OpenSSL使用硬件加速
openssl speed -evp aes-256-gcm

# 3. 监控TLS握手延迟
# 查看Prometheus指标: tls_handshake_duration_seconds
```

## 安全最佳实践

### 文件权限

```bash
# 保护私钥文件
sudo chmod 600 /etc/letsencrypt/live/*/privkey.pem
sudo chown peerlink:peerlink /etc/letsencrypt/live/*/privkey.pem

# 证书文件可读
sudo chmod 644 /etc/letsencrypt/live/*/fullchain.pem
```

### TLS配置强化

```toml
[signaling.tls]
enabled = true
cert_path = "/etc/letsencrypt/live/your-domain.com/fullchain.pem"
key_path = "/etc/letsencrypt/live/your-domain.com/privkey.pem"

# 仅允许TLS 1.3
min_tls_version = "1.3"
max_tls_version = "1.3"

# 强密码套件
cipher_suites = "TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256"

# 启用HSTS
hsts_enabled = true
hsts_max_age = 31536000  # 1年

# 启用OCSP Stapling
ocsp_stapling = true
```

## 监控证书过期

创建监控脚本 `/usr/local/bin/check_cert_expiry.sh`：

```bash
#!/bin/bash
CERT_PATH="/etc/letsencrypt/live/your-domain.com/fullchain.pem"
EXPIRY_DATE=$(openssl x509 -in "$CERT_PATH" -noout -enddate | cut -d= -f2)
EXPIRY_EPOCH=$(date -d "$EXPIRY_DATE" +%s)
NOW_EPOCH=$(date +%s)
DAYS_LEFT=$(( ($EXPIRY_EPOCH - $NOW_EPOCH) / 86400 ))

echo "Certificate expires in $DAYS_LEFT days"

if [ $DAYS_LEFT -lt 30 ]; then
    echo "WARNING: Certificate expires soon!"
    # 发送告警通知
fi
```

## 推荐配置

### 开发环境

```toml
[signaling.tls]
enabled = true
cert_path = "certs/server.crt"
key_path = "certs/server.key"
```

### 生产环境

```toml
[signaling.tls]
enabled = true
cert_path = "/etc/letsencrypt/live/your-domain.com/fullchain.pem"
key_path = "/etc/letsencrypt/live/your-domain.com/privkey.pem"
min_tls_version = "1.3"
max_tls_version = "1.3"
cipher_suites = "TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256"
hsts_enabled = true
hsts_max_age = 31536000
ocsp_stapling = true
```
