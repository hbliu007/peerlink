# PeerLink 用户使用手册

> 高性能去中心化 P2P 通信平台

---

## 目录

1. [快速开始](#快速开始)
2. [安装指南](#安装指南)
3. [配置说明](#配置说明)
4. [使用指南](#使用指南)
5. [客户端 SDK 使用](#客户端-sdk-使用)
6. [常见问题解答](#常见问题解答)
7. [故障排除](#故障排除)
8. [性能优化](#性能优化)

---

## 快速开始

### 环境要求

- **Docker**: 20.10+
- **Docker Compose**: 2.0+
- **Python**: 3.11+ (仅客户端 SDK 需要)
- **操作系统**: Linux / macOS / Windows

### 30 秒快速启动

```bash
# 1. 克隆项目
git clone https://github.com/hbliu007/peerlink.git
cd peerlink

# 2. 启动所有服务
docker-compose up -d

# 3. 检查状态
docker-compose ps

# 4. 查看日志
docker-compose logs -f
```

### 服务访问地址

启动成功后，各服务可通过以下地址访问：

| 服务 | 协议 | 地址 | 端口 |
|------|------|------|------|
| STUN 服务器 | UDP | `localhost` | 3478 |
| Relay 服务器 | UDP/TCP | `localhost` | 50000-50010 |
| 信令服务器 | WS | `ws://localhost` | 8080 |
| 信令服务器 | WSS | `wss://localhost` | 8443 |
| DID 服务 | HTTP | `http://localhost` | 9000 |

---

## 安装指南

### 方式一: Docker Compose (推荐)

**适用场景**: 生产环境部署、完整功能测试

```bash
# 启动所有服务
docker-compose up -d

# 指定配置文件
docker-compose -f docker-compose.prod.yml up -d

# 查看日志
docker-compose logs -f [service_name]

# 停止服务
docker-compose down

# 停止并删除数据卷
docker-compose down -v
```

### 方式二: RPM 包安装 (CentOS/RHEL/Fedora)

**适用场景**: Linux 服务器生产环境

```bash
# 下载最新版本
wget https://github.com/hbliu007/peerlink/releases/download/v1.0.0/peerlink-1.0.0-1.el7.x86_64.rpm

# 安装
sudo rpm -ivh peerlink-1.0.0-1.el7.x86_64.rpm

# 启动服务
sudo systemctl start peerlink-stun
sudo systemctl start peerlink-relay
sudo systemctl start peerlink-signaling
sudo systemctl start peerlink-did

# 开机自启
sudo systemctl enable peerlink-*

# 查看状态
sudo systemctl status peerlink-*
```

### 方式三: DEB 包安装 (Ubuntu/Debian)

**适用场景**: Debian 系 Linux 发行版

```bash
# 下载最新版本
wget https://github.com/hbliu007/peerlink/releases/download/v1.0.0/peerlink_1.0.0_amd64.deb

# 安装
sudo dpkg -i peerlink_1.0.0_amd64.deb

# 修复依赖 (如有必要)
sudo apt-get install -f

# 启动服务
sudo systemctl start peerlink-stun
sudo systemctl start peerlink-relay
sudo systemctl start peerlink-signaling
sudo systemctl start peerlink-did
```

### 方式四: 源码编译

**适用场景**: 开发环境、自定义构建

#### 前置依赖

**macOS**:
```bash
# 安装 Xcode Command Line Tools
xcode-select --install

# 安装 Homebrew
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# 安装依赖
brew install cmake openssl@3 protobuf spdlog boost
```

**Ubuntu/Debian**:
```bash
sudo apt update
sudo apt install -y build-essential cmake \
    libssl-dev libprotobuf-dev protobuf-compiler \
    libspdlog-dev libboost-all-dev
```

**编译 C++ 服务**:
```bash
cd p2p-cpp
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install
```

**安装 Python 服务**:
```bash
pip install -r requirements.txt
pip install -e .
```

### 方式五: Client SDK 安装

**适用场景**: 应用集成、客户端开发

```bash
# 从 PyPI 安装
pip install peerlink-sdk

# 从 Conda 安装
conda install -c conda-forge peerlink-sdk

# 从源码安装
git clone https://github.com/hbliu007/peerlink.git
cd peerlink/client_sdk
pip install -e .
```

---

## 配置说明

### Docker Compose 配置

编辑 `docker-compose.yml` 或创建自定义配置文件：

```yaml
version: '3.8'

services:
  stun:
    image: peerlink/stun:latest
    ports:
      - "3478:3478/udp"   # STUN UDP
      - "3479:3479/tcp"   # STUN TCP
    environment:
      - STUN_LOG_LEVEL=info
      - STUN_REALM=peerlink
    restart: unless-stopped

  relay:
    image: peerlink/relay:latest
    ports:
      - "50000-50010:50000-50010/udp"
      - "50000-50010:50000-50010/tcp"
    environment:
      - RELAY_EXTERNAL_IP=auto
      - RELAY_MIN_PORT=50000
      - RELAY_MAX_PORT=50010
      - RELAY_MAX_BANDWIDTH=104857600  # 100MB/s
    restart: unless-stopped

  signaling:
    image: peerlink/signaling:latest
    ports:
      - "8080:8080"   # WebSocket
      - "8443:8443"   # Secure WebSocket
    environment:
      - SIGNACING_BIND_HOST=0.0.0.0
      - SIGNACING_WS_PORT=8080
      - SIGNACING_WSS_PORT=8443
      - SIGNACING_TLS_CERT=/certs/server.crt
      - SIGNACING_TLS_KEY=/certs/server.key
    volumes:
      - ./certs:/certs:ro
    restart: unless-stopped

  did:
    image: peerlink/did:latest
    ports:
      - "9000:9000"
    environment:
      - DID_DATABASE_URL=postgresql://user:pass@db:5432/peerlink
      - DID_JWT_SECRET=your-secret-key
    restart: unless-stopped
```

### 环境变量配置

创建 `.env` 文件：

```bash
# STUN 服务器配置
STUN_SERVER=localhost
STUN_PORT=3478
STUN_REALM=peerlink

# Relay 服务器配置
RELAY_SERVER=localhost
RELAY_PORT_RANGE=50000-50010
RELAY_EXTERNAL_IP=auto
RELAY_MAX_BANDWIDTH=104857600

# 信令服务器配置
SIGNALING_SERVER=localhost
SIGNALING_WS_PORT=8080
SIGNALING_WSS_PORT=8443

# DID 服务配置
DID_SERVER=localhost
DID_PORT=9000
DID_JWT_SECRET=your-jwt-secret
DID_DATABASE_URL=sqlite:///data/peerlink.db

# 日志配置
LOG_LEVEL=info
LOG_FORMAT=json
```

### Nginx 反向代理配置

生产环境推荐使用 Nginx：

```nginx
upstream signaling_backend {
    server localhost:8080;
}

upstream did_backend {
    server localhost:9000;
}

server {
    listen 443 ssl http2;
    server_name peerlink.example.org;

    ssl_certificate /etc/ssl/certs/peerlink.crt;
    ssl_certificate_key /etc/ssl/private/peerlink.key;

    # WebSocket 升级
    location /ws {
        proxy_pass http://signaling_backend;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        proxy_read_timeout 86400;
    }

    # DID API
    location /api/did {
        proxy_pass http://did_backend;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }

    # 健康检查
    location /health {
        access_log off;
        return 200 "healthy\n";
        add_header Content-Type text/plain;
    }
}
```

---

## 使用指南

### STUN 服务器使用

STUN 服务器用于 NAT 穿透和公网 IP 检测：

```python
import socket

# 测试 STUN 服务器
def test_stun(server, port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(5)

    # 发送 STUN 绑定请求
    stun_request = bytes.fromhex("000100002112a442")
    sock.sendto(stun_request, (server, port))

    # 接收响应
    data, addr = sock.recvfrom(1024)
    print(f"Public address: {addr}")

# 使用 PeerLink STUN
test_stun("localhost", 3478)
```

### Relay 服务器使用

当 P2P 直连失败时，Relay 服务器提供中继服务：

```python
from peerlink_sdk import P2PClient

# 自动回退到中继
client = P2PClient(
    did="device-001",
    relay_server="localhost",
    relay_port=50000,
    auto_relay=True  # 启用自动中继
)

await client.initialize()
await client.connect("device-002")  # 自动使用中继
```

### 信令服务器使用

信令服务器协调 P2P 连接建立：

```python
import asyncio
import websockets

async def connect_to_signaling():
    uri = "ws://localhost:8080"
    async with websockets.connect(uri) as websocket:
        # 注册设备
        await websocket.send(json.dumps({
            "type": "register",
            "device_id": "device-001"
        }))

        # 接收消息
        while True:
            message = await websocket.recv()
            print(f"Received: {message}")

asyncio.run(connect_to_signaling())
```

### DID 服务使用

DID 服务提供设备身份认证：

```bash
# 注册新设备
curl -X POST http://localhost:9000/api/did/register \
  -H "Content-Type: application/json" \
  -d '{"device_id": "device-001", "public_key": "..."}'

# 获取访问令牌
curl -X POST http://localhost:9000/api/did/token \
  -H "Content-Type: application/json" \
  -d '{"device_id": "device-001", "signature": "..."}'

# 验证令牌
curl -X GET http://localhost:9000/api/did/verify \
  -H "Authorization: Bearer <token>"
```

---

## 客户端 SDK 使用

### Python SDK 快速开始

```python
import asyncio
from peerlink_sdk import P2PClient

async def main():
    # 创建客户端
    client = P2PClient(
        did="device-001",
        signaling_server="localhost",
        signaling_port=8443,
        stun_server="localhost",
        stun_port=3478,
        auto_relay=True
    )

    # 初始化
    await client.initialize()
    print(f"NAT Type: {client.nat_type}")
    print(f"Local Address: {client.local_address}")

    # 连接到对等设备
    if await client.connect("device-002"):
        print("Connected!")

        # 发送数据
        await client.send_data(b"Hello, P2P!")

        # 接收数据
        data = await client.recv_data(timeout=10)
        print(f"Received: {data}")

    # 清理资源
    await client.close()

asyncio.run(main())
```

### 多通道通信

```python
from peerlink_sdk import P2PClient, ChannelType

async def multi_channel_example():
    client = P2PClient(did="device-001")
    await client.initialize()

    if await client.connect("device-002"):
        # 创建不同类型的通道
        control_ch = client.create_channel(ChannelType.CONTROL)
        video_ch = client.create_channel(ChannelType.VIDEO, priority=10)
        data_ch = client.create_channel(ChannelType.DATA)

        # 在不同通道上发送数据
        await client.send_data(control_ch, b"ping")
        await client.send_data(video_ch, video_frame)
        await client.send_data(data_ch, file_data)

    await client.close()
```

### 事件处理

```python
async def event_handling_example():
    client = P2PClient(did="device-001")

    # 注册事件处理器
    @client.on_connected
    async def on_connected():
        print("Connection established!")

    @client.on_disconnected
    async def on_disconnected():
        print("Connection lost")

    @client.on_data
    async def on_data(channel_id: int, data: bytes):
        print(f"Received {len(data)} bytes on channel {channel_id}")

    @client.on_error
    async def on_error(error: Exception):
        print(f"Error: {error}")

    await client.initialize()
    await client.connect("device-002")
```

---

## 常见问题解答

### Q1: Docker 容器启动失败？

**A**: 检查端口是否被占用：

```bash
# 检查端口占用
lsof -i :3478
lsof -i :8080
lsof -i :9000

# 或使用 netstat
netstat -tunlp | grep -E '3478|8080|9000'
```

解决方法：修改 `docker-compose.yml` 中的端口映射。

### Q2: 如何查看服务日志？

**A**:

```bash
# 查看所有服务日志
docker-compose logs -f

# 查看特定服务日志
docker-compose logs -f stun
docker-compose logs -f relay
docker-compose logs -f signaling
docker-compose logs -f did

# 查看最近 100 行日志
docker-compose logs --tail=100 [service]
```

### Q3: P2P 连接失败怎么办？

**A**: 按以下步骤排查：

1. **检查 NAT 类型**:
```python
from peerlink_sdk import detect_nat_type
nat_type = await detect_nat_type()
print(f"NAT Type: {nat_type}")
```

2. **启用自动中继**:
```python
client = P2PClient(auto_relay=True)
```

3. **检查防火墙**:
```bash
# 开放必要端口
sudo ufw allow 3478/udp
sudo ufw allow 50000:50010/udp
sudo ufw allow 8080/tcp
```

4. **验证信令服务器连接**:
```bash
wscat -c ws://localhost:8080
```

### Q4: 如何配置 HTTPS/WSS？

**A**:

1. **生成 SSL 证书**:
```bash
# 自签名证书（仅用于测试）
openssl req -x509 -newkey rsa:4096 -keyout key.pem -out cert.pem -days 365 -nodes

# Let's Encrypt（生产环境）
sudo certbot certonly --standalone -d peerlink.example.org
```

2. **配置 Nginx** (参考上面的 Nginx 配置)

3. **更新客户端连接**:
```python
client = P2PClient(
    signaling_server="peerlink.example.org",
    signaling_port=443,
    use_ssl=True
)
```

### Q5: 服务性能如何优化？

**A**:

1. **调整 Docker 资源限制**:
```yaml
services:
  relay:
    deploy:
      resources:
        limits:
          cpus: '2'
          memory: 2G
        reservations:
          cpus: '1'
          memory: 1G
```

2. **启用多进程**:
```python
import multiprocessing
workers = multiprocessing.cpu_count() * 2 + 1
```

3. **使用负载均衡**:
```bash
# 启动多个 Relay 实例
docker-compose up --scale relay=3
```

### Q6: 数据如何备份？

**A**:

```bash
# 备份 Docker 卷
docker run --rm -v peerlink_data:/data -v $(pwd):/backup \
    alpine tar czf /backup/peerlink-backup.tar.gz /data

# 恢复
docker run --rm -v peerlink_data:/data -v $(pwd):/backup \
    alpine tar xzf /backup/peerlink-backup.tar.gz -C /
```

### Q7: 如何升级版本？

**A**:

```bash
# 拉取最新镜像
docker-compose pull

# 重新构建并启动
docker-compose up -d --build

# 数据库迁移（如有）
python scripts/migrate.py
```

---

## 故障排除

### 连接问题诊断脚本

```python
import asyncio
import socket
from peerlink_sdk import P2PClient, detect_nat_type

async def diagnose():
    print("=== PeerLink 诊断工具 ===\n")

    # 1. 检查网络连接
    print("1. 检查网络连接...")
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.connect(("8.8.8.8", 53))
        print("   ✓ 网络连接正常")
    except Exception as e:
        print(f"   ✗ 网络连接失败: {e}")
        return

    # 2. 检查 STUN 服务器
    print("\n2. 检查 STUN 服务器...")
    try:
        nat_type = await detect_nat_type("localhost", 3478)
        print(f"   ✓ STUN 服务器正常")
        print(f"   NAT 类型: {nat_type}")
    except Exception as e:
        print(f"   ✗ STUN 服务器不可用: {e}")

    # 3. 检查信令服务器
    print("\n3. 检查信令服务器...")
    try:
        client = P2PClient(signaling_server="localhost", signaling_port=8080)
        await client.initialize()
        print(f"   ✓ 信令服务器连接成功")
        await client.close()
    except Exception as e:
        print(f"   ✗ 信令服务器连接失败: {e}")

    # 4. 检查 DID 服务
    print("\n4. 检查 DID 服务...")
    try:
        import aiohttp
        async with aiohttp.ClientSession() as session:
            async with session.get("http://localhost:9000/health") as resp:
                if resp.status == 200:
                    print("   ✓ DID 服务正常")
    except Exception as e:
        print(f"   ✗ DID 服务不可用: {e}")

    print("\n=== 诊断完成 ===")

asyncio.run(diagnose())
```

### 日志级别调整

```bash
# Docker Compose
environment:
  - LOG_LEVEL=debug

# Python SDK
import logging
logging.getLogger("peerlink").setLevel(logging.DEBUG)
```

### 常见错误代码

| 错误代码 | 说明 | 解决方案 |
|----------|------|----------|
| `E001` | NAT 检测失败 | 检查 STUN 服务器 |
| `E002` | 信令服务器连接失败 | 检查网络和防火墙 |
| `E003` | P2P 连接超时 | 增加超时时间或启用中继 |
| `E004` | 认证失败 | 检查设备 ID 和密钥 |
| `E005` | 中继服务器不可用 | 检查 Relay 服务器状态 |

---

## 性能优化

### 网络优化

```python
# 根据网络条件调整参数
client = P2PClient(
    # 良好网络
    connection_timeout=10.0,
    punch_timeout=5.0,

    # 一般网络
    # connection_timeout=30.0,
    # punch_timeout=10.0,

    # 差网络
    # connection_timeout=60.0,
    # punch_timeout=20.0,
)
```

### 缓冲区优化

```python
# 小数据量（控制消息）
client = P2PClient(recv_buffer_size=4096)

# 大数据量（视频流）
client = P2PClient(recv_buffer_size=131072)  # 128KB
```

### 批量发送优化

```python
# 不推荐：多次小消息
for i in range(1000):
    await client.send_data(channel, f"msg{i}".encode())

# 推荐：批量发送
messages = [f"msg{i}".encode() for i in range(1000)]
batch = b"\n".join(messages)
await client.send_data(channel, batch)
```

---

## 更多资源

- [架构设计文档](./architecture.md)
- [API 规范](./api-spec.md)
- [开发者指南](./DEVELOPER_GUIDE.md)
- [测试指南](./TESTING.md)
- [部署指南](../DEPLOYMENT.md)
- [运维指南](../OPERATIONS.md)

---

**文档版本**: 1.0
**最后更新**: 2026-03-18
