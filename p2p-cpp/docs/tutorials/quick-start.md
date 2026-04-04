# 快速开始

5 分钟快速部署 PeerLink 并建立第一个 P2P 连接。

---

## 前提条件

- Docker 和 Docker Compose
- 或 C++20 编译器（GCC 11+ / Clang 13+ / MSVC 2022+）
- 两个设备用于测试（可以是两台电脑，或一台电脑 + 一台服务器）

---

## 方案 1: Docker 部署（推荐）

### 步骤 1: 部署服务端

```bash
# 克隆仓库
git clone https://github.com/your-org/peerlink.git
cd peerlink

# 启动所有服务（Signaling + STUN + Relay）
docker-compose up -d

# 查看服务状态
docker-compose ps
```

服务包括：
- **Signaling Server**: 端口 8443 (WSS)
- **STUN Server**: 端口 3478 (UDP/TCP)
- **Relay Server**: 端口 443 (TLS)

### 步骤 2: 安装客户端

```bash
# macOS / Linux
curl -fsSL https://get.peerlink.io | sh

# 或使用 Docker
docker run -d --name peerlink-client \
  -v ~/.peerlink:/root/.peerlink \
  --network host \
  peerlink/client:latest daemon
```

### 步骤 3: 初始化配置

```bash
# 创建配置文件
mkdir -p ~/.peerlink
cat > ~/.peerlink/config.yaml << EOF
signaling_server: wss://your-server.com:8443
stun_server: stun:your-server.com:3478
relay_server: your-server.com:443
EOF
```

### 步骤 4: 启动守护进程

```bash
# 启动守护进程
peerlink daemon

# 查看日志
peerlink logs
```

### 步骤 5: 获取 Peer ID

```bash
# 查看自己的 Peer ID
peerlink whoami

# 输出示例:
# Your Peer ID: did:peer:1zQmWvQxTqbGvZGh7Yh4z7Yh4z7Yh4z7Yh4z7Yh4z7Yh4z7
```

### 步骤 6: 建立连接

在**设备 A** 上：

```bash
# 启动监听
peerlink listen
```

在**设备 B** 上：

```bash
# 连接到设备 A（替换为实际的 Peer ID）
peerlink connect did:peer:1zQmWvQxTqbGvZGh7Yh4z7Yh4z7Yh4z7Yh4z7Yh4z7Yh4z7
```

---

## 方案 2: 从源码构建

### 步骤 1: 编译

```bash
# 克隆仓库
git clone https://github.com/your-org/peerlink.git
cd peerlink

# 创建构建目录
mkdir build && cd build

# 配置和编译
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 安装
sudo make install
```

### 步骤 2: 配置环境

```bash
# 设置环境变量
export PEERLINK_SIGNALING_SERVER=wss://your-server.com:8443
export PEERLINK_STUN_SERVER=stun:your-server.com:3478
export PEERLINK_RELAY_SERVER=your-server.com:443
```

### 步骤 3: 运行

```bash
# 启动守护进程
peerlink-daemon

# 其他步骤与方案 1 相同
```

---

## 验证连接

连接成功后，你会看到类似输出：

```
[INFO] Connected to peer: did:peer:1zQmWvQxTqbGvZGh7Yh4z7Yh4z7Yh4z7Yh4z7Yh4z7Yh4z7
[INFO] Connection type: DIRECT (UDP)
[INFO] Latency: 8ms
[INFO] Bandwidth: 543 Mbps
```

### 测试数据传输

```bash
# 在设备 A 上（监听端）
peerlink receive

# 在设备 B 上（连接端）
echo "Hello, PeerLink!" | peerlink send did:peer:1zQmWvQxTqbGvZGh7Yh4z7Yh4z7Yh4z7Yh4z7Yh4z7Yh4z7
```

---

## 常见问题

### Q: 连接失败怎么办？

```bash
# 查看详细日志
peerlink logs --verbose

# 测试 Signaling 服务器连接
peerlink test signaling

# 测试 STUN 服务器连接
peerlink test stun

# 检查 NAT 类型
peerlink detect-nat
```

### Q: 如何查看连接列表？

```bash
peerlink list
```

### Q: 如何断开连接？

```bash
peerlink disconnect <session-id>
```

---

## 下一步

- 📖 [了解工作原理](../concepts/how-peerlink-works.md)
- 🚀 [完整部署指南](deployment.md)
- 🔧 [配置选项](../how-to/configuration.md)
