# P2P Tunnel

简化版P2P端口转发工具，用于通过P2P连接转发TCP流量。

## 功能特性

- 基于PeerLink P2P平台的TCP端口转发
- 支持NAT穿透和自动中继降级
- 单TCP连接转发（MVP版本）
- 简单的命令行接口

## 编译

```bash
cd /path/to/p2p-cpp
mkdir -p build && cd build
cmake .. -DBUILD_TESTS=OFF -DBUILD_EXAMPLES=OFF
make p2p-tunnel-client p2p-tunnel-server -j8
```

编译后的可执行文件位于 `build/tools/p2p-tunnel/`。

## 使用方法

### 服务端（办公室电脑）

在办公室电脑上运行服务端，将P2P流量转发到本地Claude Code端口：

```bash
./p2p-tunnel-server office-001 127.0.0.1 8080 ws://<YOUR_SERVER_IP>:8080 <YOUR_SERVER_IP>:3478
```

参数说明：
- `office-001`: 服务端设备ID
- `127.0.0.1`: 要转发到的目标主机
- `8080`: 要转发到的目标端口（Claude Code监听端口）
- `ws://<YOUR_SERVER_IP>:8080`: Signaling服务器地址（可选，默认localhost:8080）
- `<YOUR_SERVER_IP>:3478`: STUN服务器地址（可选，默认stun.l.google.com:19302）

### 客户端（手机）

在手机上运行客户端，监听本地端口并通过P2P转发到服务端：

```bash
./p2p-tunnel-client mobile-001 office-001 9000 8080 ws://<YOUR_SERVER_IP>:8080 <YOUR_SERVER_IP>:3478
```

参数说明：
- `mobile-001`: 客户端设备ID
- `office-001`: 服务端设备ID
- `9000`: 本地监听端口
- `8080`: 远程目标端口
- `ws://<YOUR_SERVER_IP>:8080`: Signaling服务器地址（可选）
- `<YOUR_SERVER_IP>:3478`: STUN服务器地址（可选）

### 连接Claude Code

1. 在办公室电脑启动服务端
2. 在手机启动客户端
3. 在手机Claude Code中配置连接到 `127.0.0.1:9000`

## 架构说明

```
手机Claude Code → 127.0.0.1:9000 → P2P通道 → 办公室电脑 → 127.0.0.1:8080 → 办公室Claude Code
```

## 限制

当前MVP版本的限制：
- 仅支持单TCP连接（不支持多连接复用）
- 无心跳保活机制
- 无自动重连功能
- 无多端口转发

## 依赖

- PeerLink P2P平台核心库
- Boost.Asio
- C++17/20编译器

## 故障排查

### 连接失败

1. 检查Signaling服务器是否运行
2. 检查STUN服务器是否可达
3. 查看日志输出的NAT类型和连接状态

### 数据传输失败

1. 检查P2P通道是否建立成功
2. 检查目标端口是否正确
3. 查看错误日志

## 后续改进

- [ ] 支持多TCP连接复用
- [ ] 添加心跳保活机制
- [ ] 实现自动重连
- [ ] 支持多端口转发
- [ ] 添加TLS加密
- [ ] 性能优化和缓冲区管理
