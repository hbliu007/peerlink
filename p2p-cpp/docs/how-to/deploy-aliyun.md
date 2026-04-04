---
title: 阿里云部署指南
description: 在阿里云ECS上部署PeerLink生产环境的完整指南
tags:
  - deployment
  - aliyun
  - production
  - systemd
  - nginx
---

# 阿里云部署指南

本指南介绍如何在阿里云ECS上部署PeerLink生产环境。PeerLink采用原生Linux服务部署模式，不依赖Docker。

## 前置要求

- 阿里云ECS实例（推荐Ubuntu 22.04）
- Root或sudo权限
- 域名（可选，用于TLS证书）

## 主机准备

### 1. 安装依赖包

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libboost-all-dev libssl-dev \
  protobuf-compiler libprotobuf-dev nlohmann-json3-dev redis-server nginx
```

### 2. 创建运行用户和目录

```bash
# 创建运行用户
sudo useradd --system --home /var/lib/peerlink --shell /usr/sbin/nologin peerlink || true

# 创建目录结构
sudo mkdir -p /opt/peerlink/bin
sudo mkdir -p /etc/peerlink
sudo mkdir -p /var/lib/peerlink

# 设置权限
sudo chown -R peerlink:peerlink /opt/peerlink /var/lib/peerlink
```

## 编译和安装

### 1. 编译源码

```bash
# 克隆或进入源码目录
cd /path/to/p2p-cpp

# 配置构建
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_SERVERS=ON

# 编译服务程序
cmake --build build --target peerlink-gateway peerlink-network simple-relay-server -j"$(nproc)"
```

### 2. 安装二进制文件

```bash
# 安装到系统目录
sudo install -m 0755 build/src/servers/gateway/peerlink-gateway /opt/peerlink/bin/
sudo install -m 0755 build/src/servers/network/peerlink-network /opt/peerlink/bin/
sudo install -m 0755 build/tools/p2p-tunnel/simple-relay-server /opt/peerlink/bin/

# 安装配置文件
sudo install -m 0644 config/peerlink.aliyun.example.toml /etc/peerlink/peerlink.toml
```

## 配置文件

编辑 `/etc/peerlink/peerlink.toml`：

```toml
# 信令服务器配置
[signaling]
host = "127.0.0.1"
port = 18080
heartbeat_interval = 30
connection_timeout = 90
allow_insecure_registration = false

# DID服务器配置
[did]
host = "127.0.0.1"
port = 18081
max_connections = 1000

# Redis配置
[redis]
host = "127.0.0.1"
port = 6379

# 认证配置 - 必须修改！
[auth]
jwt_secret = "请替换为随机生成的密钥"

# STUN服务器配置
[stun]
host = "0.0.0.0"
udp_port = 3478
tcp_port = 3479

# TURN中继服务器配置
[turn]
host = "0.0.0.0"
port = 9001
public_ip = "<YOUR_SERVER_IP>"  # 替换为你的公网IP
min_port = 50000
max_port = 50100
lifetime = 600
max_allocations = 1000

# Gateway管理接口
[gateway.admin]
enabled = true
host = "127.0.0.1"
port = 9300

# Network服务管理接口
[network.admin]
enabled = true
host = "127.0.0.1"
port = 9301

# Signaling管理接口
[signaling.admin]
enabled = true
host = "127.0.0.1"
port = 9302

# DID管理接口
[did.admin]
enabled = true
host = "127.0.0.1"
port = 9303
```

!!! warning "安全提示"
    - 必须修改 `jwt_secret` 为强随机密钥
    - 保持 `allow_insecure_registration = false`
    - Admin端口仅绑定127.0.0.1

## Systemd服务配置

### Gateway服务

创建 `/etc/systemd/system/peerlink-gateway.service`：

```ini
[Unit]
Description=PeerLink Gateway Service
After=network-online.target redis-server.service
Wants=network-online.target

[Service]
Type=simple
User=peerlink
Group=peerlink
Environment=PEERLINK_CONFIG=/etc/peerlink/peerlink.toml
WorkingDirectory=/var/lib/peerlink
ExecStart=/opt/peerlink/bin/peerlink-gateway
Restart=always
RestartSec=3
NoNewPrivileges=true
LimitNOFILE=65535

[Install]
WantedBy=multi-user.target
```

### Network服务

创建 `/etc/systemd/system/peerlink-network.service`：

```ini
[Unit]
Description=PeerLink Network Service
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=peerlink
Group=peerlink
Environment=PEERLINK_CONFIG=/etc/peerlink/peerlink.toml
WorkingDirectory=/var/lib/peerlink
ExecStart=/opt/peerlink/bin/peerlink-network
Restart=always
RestartSec=3
NoNewPrivileges=true
LimitNOFILE=65535

[Install]
WantedBy=multi-user.target
```

### Simple Relay服务

创建 `/etc/systemd/system/peerlink-simple-relay.service`：

```ini
[Unit]
Description=PeerLink Simple TCP Relay (for relay-tunnel)
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=peerlink
Group=peerlink
WorkingDirectory=/var/lib/peerlink
ExecStart=/opt/peerlink/bin/simple-relay-server --host 127.0.0.1 --port 9700
Restart=always
RestartSec=3
NoNewPrivileges=true
LimitNOFILE=65535

[Install]
WantedBy=multi-user.target
```

### 启动服务

```bash
# 重载systemd配置
sudo systemctl daemon-reload

# 启用服务
sudo systemctl enable peerlink-gateway peerlink-network peerlink-simple-relay

# 启动服务
sudo systemctl start peerlink-gateway peerlink-network peerlink-simple-relay

# 查看状态
sudo systemctl status peerlink-gateway
sudo systemctl status peerlink-network
sudo systemctl status peerlink-simple-relay
```

## Nginx配置

### HTTPS/WSS入口配置

创建 `/etc/nginx/sites-available/peerlink-gateway.conf`：

```nginx
server {
    listen 443 ssl http2;
    server_name your-domain.com;  # 替换为你的域名

    ssl_certificate /etc/letsencrypt/live/your-domain.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/your-domain.com/privkey.pem;
    ssl_protocols TLSv1.3;
    ssl_ciphers TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256:TLS_AES_128_GCM_SHA256;

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

### TCP Stream配置（用于relay-tunnel）

创建 `/etc/nginx/peerlink-stream.conf`：

```nginx
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

在 `/etc/nginx/nginx.conf` 末尾添加：

```nginx
# 在http { }块之外添加
include /etc/nginx/peerlink-stream.conf;
```

### 启用配置

```bash
# 创建符号链接
sudo ln -sf /etc/nginx/sites-available/peerlink-gateway.conf /etc/nginx/sites-enabled/peerlink-gateway.conf

# 测试配置
sudo nginx -t

# 重载nginx
sudo systemctl reload nginx
```

## 端口开放清单

在阿里云安全组中开放以下端口：

| 端口 | 协议 | 用途 |
|------|------|------|
| 443 | TCP | HTTPS/WSS入口 |
| 9443 | TCP | TCP Relay（relay-tunnel） |
| 3478 | UDP | STUN服务 |
| 3479 | TCP | STUN over TCP |
| 9001 | UDP | TURN控制 |
| 50000-50100 | UDP | TURN中继数据 |

!!! warning "安全建议"
    - Admin端口（9300-9303）不要对公网开放
    - 根据实际需要限制源IP地址

## 验证部署

### 1. 检查服务状态

```bash
# 检查各服务健康状态
curl -fsS http://127.0.0.1:9300/healthz
curl -fsS http://127.0.0.1:9301/healthz
curl -fsS http://127.0.0.1:9302/healthz
curl -fsS http://127.0.0.1:9303/healthz
```

### 2. 检查HTTPS访问

```bash
# 检查nginx HTTPS
curl -k https://127.0.0.1/
```

### 3. 检查Simple Relay

```bash
# 直接测试relay
printf 'REGISTER test\n' | nc -w 2 127.0.0.1 9700

# 通过nginx测试
printf 'REGISTER test\n' | nc -w 2 127.0.0.1 9443
```

预期返回：`OK test-registered`

### 4. 检查端口监听

```bash
ss -lntup | rg '18080|18081|3478|3479|9001|9300|9301|9443|9700'
```

### 5. 查看服务日志

```bash
# 实时查看gateway日志
sudo journalctl -u peerlink-gateway -f

# 实时查看network日志
sudo journalctl -u peerlink-network -f

# 实时查看simple-relay日志
sudo journalctl -u peerlink-simple-relay -f
```

## 生产环境检查清单

部署完成后，确认以下项目：

- [ ] 已修改JWT_SECRET为强随机密钥
- [ ] `allow_insecure_registration` 设置为 `false`
- [ ] Admin端口仅绑定127.0.0.1
- [ ] TLS证书已配置
- [ ] 阿里云安全组已正确配置
- [ ] Redis服务正常运行
- [ ] 所有服务状态为active
- [ ] 健康检查端点返回正常
- [ ] 日志正常无错误

## 常见问题

### 服务启动失败

```bash
# 查看详细错误
sudo journalctl -u peerlink-gateway -n 50

# 检查配置文件
sudo -u peerlink PEERLINK_CONFIG=/etc/peerlink/peerlink.toml /opt/peerlink/bin/peerlink-gateway --check-config
```

### 端口被占用

```bash
# 查找占用进程
sudo lsof -i :18080

# 或使用ss命令
sudo ss -ltnp | grep 18080
```

### TLS证书问题

参考 [TLS配置指南](./configure-tls.md) 获取详细的TLS部署和故障排查信息。

## 升级部署

```bash
# 1. 备份当前版本
sudo cp /opt/peerlink/bin/peerlink-gateway /opt/peerlink/bin/peerlink-gateway.bak

# 2. 编译新版本
cmake --build build --target peerlink-gateway peerlink-network -j"$(nproc)"

# 3. 停止服务
sudo systemctl stop peerlink-gateway peerlink-network

# 4. 安装新版本
sudo install -m 0755 build/src/servers/gateway/peerlink-gateway /opt/peerlink/bin/
sudo install -m 0755 build/src/servers/network/peerlink-network /opt/peerlink/bin/

# 5. 启动服务
sudo systemctl start peerlink-gateway peerlink-network

# 6. 验证
sudo systemctl status peerlink-gateway peerlink-network
```
