---
title: Python SDK 快速入门
description: 使用 PeerLink Python SDK 快速构建 P2P 应用
tags:
  - 教程
  - Python
  - SDK
  - 快速入门
---

# Python SDK 快速入门

本教程展示如何使用 PeerLink Python SDK 构建点对点通信应用。

---

## 安装

```bash
pip install peerlink-sdk
```

!!! tip "从源码安装"
    如果需要最新开发版本：
    ```bash
    git clone https://github.com/your-org/peerlink.git
    cd peerlink
    pip install -e .
    ```

---

## 创建客户端

### 基本配置

PeerLink 使用 `P2PConfig` 结构配置客户端参数：

```python
from peerlink import P2PClient, P2PConfig, RelayMode

# 创建配置
config = P2PConfig(
    # 信令服务器（用于交换连接信息）
    signaling_server="your-server.com",
    signaling_port=8443,

    # STUN 服务器（用于 NAT 类型检测）
    stun_server="stun.l.google.com",
    stun_port=19302,

    # Relay 服务器（中继转发，保底通道）
    tcp_relay_server="your-server.com",
    tcp_relay_port=9443,

    # 连接超时（秒）
    connection_timeout=30,

    # 打洞超时（秒）
    punch_timeout=10,

    # 保活间隔（秒）
    keepalive_interval=5,

    # 最大重试次数
    max_retries=3,

    # 中继模式
    relay_mode=RelayMode.AUTO,
)
```

### 配置参数说明

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `signaling_server` | str | `"localhost"` | 信令服务器地址 |
| `signaling_port` | int | `8443` | 信令服务器端口 |
| `stun_server` | str | `"stun.l.google.com"` | STUN 服务器地址 |
| `stun_port` | int | `19302` | STUN 服务器端口 |
| `tcp_relay_server` | str | `""` | TCP Relay 服务器地址 |
| `tcp_relay_port` | int | `9700` | TCP Relay 端口 |
| `connection_timeout` | int | `30` | 连接超时（秒） |
| `punch_timeout` | int | `10` | 打洞超时（秒） |
| `keepalive_interval` | int | `5` | 保活间隔（秒） |
| `max_retries` | int | `3` | 最大重试次数 |
| `relay_mode` | RelayMode | `AUTO` | 中继模式选择 |

### RelayMode 选择

| 模式 | 说明 | 适用场景 |
|------|------|---------|
| `AUTO` | 先尝试直连，失败后回退到 Relay | 通用场景（推荐） |
| `RELAY_PREFERRED` | 优先使用 Relay，Relay 不可用时尝试直连 | 需要稳定连接 |
| `RELAY_ONLY` | 跳过打洞，直接使用 Relay | 企业防火墙后 |
| `DIRECT_ONLY` | 不使用 Relay | 仅局域网或已知可达环境 |

---

## 初始化和连接

```python
from peerlink import P2PClient, P2PConfig, RelayMode

# 创建配置
config = P2PConfig(
    signaling_server="peerlink.example.com",
    tcp_relay_server="peerlink.example.com",
    tcp_relay_port=9443,
    relay_mode=RelayMode.AUTO,
)

# 创建客户端（参数：io_context, 设备 DID, 配置）
# DID 是设备的唯一标识，自定义字符串
client = P2PClient(
    did="my-device-001",
    config=config,
)

# 初始化（NAT 检测 + 信令连接）
await client.initialize()

# 连接到远程设备
await client.connect("remote-device-002")
```

---

## 创建数据通道

PeerLink 支持在一条连接上创建多个独立的数据通道，实现多路复用。

```python
# 创建通道（返回通道 ID）
channel_id = client.create_channel()
print(f"Created channel: {channel_id}")

# 创建多个通道用于不同用途
ssh_channel = client.create_channel()     # SSH 数据
file_channel = client.create_channel()    # 文件传输
control_channel = client.create_channel() # 控制消息
```

---

## 发送和接收数据

### 发送数据

```python
# 发送文本
text_data = b"Hello, PeerLink!"
await client.send_data(channel_id, text_data)

# 发送二进制数据
binary_data = bytes([0x01, 0x02, 0x03, 0x04])
await client.send_data(channel_id, binary_data)

# 发送 JSON 消息
import json
message = json.dumps({"type": "ping", "seq": 1}).encode()
await client.send_data(channel_id, message)
```

### 接收数据

通过 `on_data` 回调接收数据：

```python
def handle_data(channel_id: int, data: bytes):
    print(f"Received {len(data)} bytes on channel {channel_id}")
    message = data.decode('utf-8', errors='replace')
    print(f"Content: {message}")

# 注册数据回调
client.on_data(handle_data)
```

---

## 事件回调

PeerLink 提供四个事件回调接口：

```python
# 连接成功
def on_connected():
    print("Connected to peer!")
    state = client.state()           # ConnectionState
    is_p2p = client.is_p2p()         # 是否为 P2P 直连
    peer = client.peer()             # PeerInfo

client.on_connected(on_connected)

# 连接断开
def on_disconnected():
    print("Disconnected from peer")

client.on_disconnected(on_disconnected)

# 数据到达
def on_data(channel_id: int, data: bytes):
    print(f"Data on channel {channel_id}: {len(data)} bytes")

client.on_data(on_data)

# 错误发生
def on_error(error_code, message: str):
    print(f"Error: {message} (code: {error_code})")

client.on_error(on_error)
```

---

## 完整示例：简易聊天

```python
import asyncio
from peerlink import P2PClient, P2PConfig, RelayMode


async def main():
    # 配置
    config = P2PConfig(
        signaling_server="peerlink.example.com",
        tcp_relay_server="peerlink.example.com",
        tcp_relay_port=9443,
        relay_mode=RelayMode.AUTO,
    )

    # 创建客户端
    client = P2PClient(did="chat-alice", config=config)

    # 注册事件回调
    client.on_connected(lambda: print("[Connected] Ready to chat!"))
    client.on_disconnected(lambda: print("[Disconnected]"))
    client.on_error(lambda ec, msg: print(f"[Error] {msg}"))

    def handle_message(channel_id: int, data: bytes):
        msg = data.decode('utf-8', errors='replace')
        print(f"[Message] {msg}")

    client.on_data(handle_message)

    # 初始化
    await client.initialize()
    print(f"My DID: {client.did()}")

    # 创建数据通道
    channel = client.create_channel()

    # 连接到对方（替换为对方的 DID）
    target_did = input("Enter peer DID: ")
    await client.connect(target_did)

    # 发送消息循环
    while True:
        try:
            text = input("> ")
            if text.lower() in ("quit", "exit"):
                break
            await client.send_data(channel, text.encode())
        except EOFError:
            break

    # 关闭
    client.close()


asyncio.run(main())
```

---

## 连接状态查询

```python
from peerlink import ConnectionState, ConnectionPath

# 查询连接状态
state = client.state()
if state == ConnectionState.CONNECTED_P2P:
    print("Connected via direct P2P")
elif state == ConnectionState.CONNECTED_RELAY:
    print("Connected via relay")
elif state == ConnectionState.CONNECTING:
    print("Connecting...")
elif state == ConnectionState.FAILED:
    print("Connection failed")

# 是否已连接
if client.is_connected():
    print("Active connection")

# 是否为 P2P 直连
if client.is_p2p():
    print("Direct P2P connection (low latency)")

# 当前连接路径
path = client.active_path()
print(f"Path: {path}")  # ConnectionPath.DIRECT_P2P or ConnectionPath.RELAY
```

---

## 关闭连接

```python
# 关闭指定通道
client.close_channel(channel_id)

# 关闭整个连接
client.close()
```

---

## 常见问题

### Q: 连接超时怎么办？

```python
# 增加超时时间
config = P2PConfig(
    connection_timeout=60,  # 60 秒
    punch_timeout=20,       # 20 秒
)

# 或直接使用 Relay 模式（跳过打洞）
config = P2PConfig(
    relay_mode=RelayMode.RELAY_ONLY,
)
```

### Q: 如何选择 RelayMode？

- **企业防火墙后**: 使用 `RELAY_ONLY`，跳过必然失败的打洞过程
- **家庭网络**: 使用 `AUTO`，先尝试直连获得更好性能
- **局域网**: 使用 `DIRECT_ONLY`，不浪费 Relay 资源

### Q: 多通道有什么用？

多通道允许在同一连接上传输不同类型的数据，互不干扰。例如同时进行 SSH 隧道和文件传输。

---

## 下一步

- [SSH 隧道教程](ssh-tunnel.md) - 用 relay-tunnel 建立远程访问
- [API 参考文档](../reference/api.md) - 完整的 API 说明
- [工作原理](../concepts/how-peerlink-works.md) - 了解底层架构
