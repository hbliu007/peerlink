---
title: Python API 参考
description: PeerLink Python SDK API参考，包括P2PClient、Config、Connection类和事件类型
tags:
  - api
  - python
  - reference
  - sdk
---

# Python API 参考

本页面提供PeerLink Python SDK的完整API参考。

## 安装

```bash
# 从源码构建
cd p2p-cpp
python -m pip install -e .

# 或安装wheel
pip install peerlink-1.0.0-py3-none-linux_x86_64.whl
```

## 快速开始

```python
import peerlink

# 创建客户端
client = peerlink.P2PClient(
    did="device-001",
    signaling_server=("127.0.0.1", 8080),
    stun_server=("127.0.0.1", 3478)
)

# 连接到对等端
client.connect("device-002")

# 发送数据
client.send(b"Hello, peer!")

# 接收数据
data = client.recv()
```

## P2PClient类

### 构造函数

```python
peerlink.P2PClient(
    did: str,
    config: Optional[Config] = None
) -> P2PClient
```

创建P2P客户端实例。

**参数：**
- `did` - 设备ID
- `config` - 配置对象（可选）

**示例：**

```python
# 使用默认配置
client = peerlink.P2PClient(did="device-001")

# 使用自定义配置
config = peerlink.Config()
config.signaling_server = ("127.0.0.1", 8080)
config.stun_server = ("127.0.0.1", 3478)
client = peerlink.P2PClient(did="device-001", config=config)
```

---

### connect()

```python
connect(
    peer_did: str,
    timeout: Optional[float] = None
) -> Connection
```

连接到对等端。

**参数：**
- `peer_did` - 对等端设备ID
- `timeout` - 超时时间（秒），None表示使用默认值

**返回值：** Connection对象

**抛出：**
- `ConnectionError` - 连接失败
- `TimeoutError` - 连接超时

**示例：**

```python
try:
    conn = client.connect("device-002", timeout=30.0)
    print(f"Connected to {peer_did}")
except peerlink.ConnectionError as e:
    print(f"Connection failed: {e}")
```

---

### send()

```python
send(
    data: bytes,
    channel_id: int = 0
) -> int
```

发送数据到对等端。

**参数：**
- `data` - 要发送的数据
- `channel_id` - 通道ID（默认0）

**返回值：** 发送的字节数

**示例：**

```python
client.send(b"Hello, peer!")
client.send(b"Binary data", channel_id=1)
```

---

### recv()

```python
recv(
    buffer_size: int = 4096,
    channel_id: int = 0
) -> bytes
```

接收来自对等端的数据。

**参数：**
- `buffer_size` - 缓冲区大小
- `channel_id` - 通道ID（默认0）

**返回值：** 接收到的数据

**示例：**

```python
data = client.recv()
print(f"Received: {data}")
```

---

### close()

```python
close() -> None
```

关闭连接。

**示例：**

```python
client.close()
```

---

### 回调方法

```python
# 设置连接回调
client.on_connected(lambda: print("Connected"))

# 设置断开回调
client.on_disconnected(lambda: print("Disconnected"))

# 设置数据回调
def on_data(data):
    print(f"Received: {data}")

client.on_data(on_data)

# 设置错误回调
def on_error(error):
    print(f"Error: {error}")

client.on_error(on_error)
```

---

### 属性

| 属性 | 类型 | 说明 |
|------|------|------|
| `did` | str | 设备ID |
| `state` | ConnectionState | 连接状态 |
| `is_connected` | bool | 是否已连接 |
| `is_p2p` | bool | 是否为P2P连接 |
| `remote_did` | str | 对等端设备ID |

## Config类

### 构造函数

```python
peerlink.Config() -> Config
```

创建配置对象。

---

### 属性

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `signaling_server` | Tuple[str, int] | ("127.0.0.1", 8080) | 信令服务器地址 |
| `stun_server` | Tuple[str, int] | ("127.0.0.1", 3478) | STUN服务器地址 |
| `relay_server` | Tuple[str, int] | ("127.0.0.1", 9001) | 中继服务器地址 |
| `local_port` | int | 0 | 本地端口（0=自动） |
| `max_connections` | int | 10 | 最大连接数 |

**示例：**

```python
config = peerlink.Config()
config.signaling_server = ("your-server.com", 443)
config.stun_server = ("your-server.com", 3478)
config.relay_server = ("your-server.com", 9001)
config.max_connections = 20
```

## Connection类

### 属性

| 属性 | 类型 | 说明 |
|------|------|------|
| `peer_did` | str | 对等端设备ID |
| `local_address` | str | 本地地址 |
| `remote_address` | str | 对等端地址 |
| `is_p2p` | bool | 是否为P2P连接 |
| `state` | ConnectionState | 连接状态 |

### 方法

```python
# 发送数据
connection.send(data: bytes) -> int

# 接收数据
connection.recv(buffer_size: int = 4096) -> bytes

# 关闭连接
connection.close() -> None
```

## 事件类型

### ConnectionState

```python
class peerlink.ConnectionState(Enum):
    DISCONNECTED = 0
    CONNECTING = 1
    HANDSHAKE = 2
    CONNECTED_P2P = 3
    CONNECTED_RELAY = 4
    FAILED = 5
```

### ConnectionEventType

```python
class peerlink.ConnectionEventType(Enum):
    CONNECTION_OPENED = 0
    CONNECTION_CLOSED = 1
    CONNECTION_ERROR = 2
    DATA_RECEIVED = 3
    PEER_DISCOVERED = 4
    NAT_TYPE_DETECTED = 5
```

### ConnectionError

```python
class peerlink.ConnectionError(Exception):
    """连接错误基类"""

class peerlink.TimeoutError(peerlink.ConnectionError):
    """连接超时"""

class peerlink.NATDetectionError(peerlink.ConnectionError):
    """NAT检测失败"""
```

## 异步API

### AsyncP2PClient

```python
import asyncio
import peerlink

async def main():
    # 创建异步客户端
    client = peerlink.AsyncP2PClient(did="device-001")

    # 连接
    conn = await client.connect_async("device-002")

    # 发送数据
    await client.send_async(b"Hello!")

    # 接收数据
    data = await client.recv_async()

    # 关闭
    await client.close_async()

asyncio.run(main())
```

## 完整示例

### 基本客户端

```python
import peerlink

# 创建配置
config = peerlink.Config()
config.signaling_server = ("your-server.com", 443)
config.stun_server = ("your-server.com", 3478)

# 创建客户端
client = peerlink.P2PClient(
    did="device-001",
    config=config
)

# 设置回调
def on_connected():
    print(f"Connected to {client.remote_did}")
    print(f"Connection type: {'P2P' if client.is_p2p else 'Relay'}")

client.on_connected(on_connected)

def on_data(data):
    print(f"Received: {data.decode()}")

client.on_data(on_data)

# 连接
try:
    client.connect("device-002")

    # 发送数据
    client.send(b"Hello, peer!")

    # 保持运行
    client.run()

except peerlink.ConnectionError as e:
    print(f"Error: {e}")

finally:
    client.close()
```

### 回显服务器

```python
import peerlink

config = peerlink.Config()
config.signaling_server = ("127.0.0.1", 8080)

server = peerlink.P2PClient(did="echo-server", config=config)

def on_data(data):
    print(f"Received: {data}")
    server.send(data)  # 回显

server.on_data(on_data)
server.run()
```

### 文件传输

```python
import peerlink

sender = peerlink.P2PClient(did="sender")
receiver = peerlink.P2PClient(did="receiver")

# 接收端
def on_file_data(data):
    with open("received.txt", "ab") as f:
        f.write(data)

receiver.on_data(on_file_data)

# 发送端
sender.connect("receiver")

with open("large_file.txt", "rb") as f:
    while True:
        chunk = f.read(4096)
        if not chunk:
            break
        sender.send(chunk)
```

## 错误处理

```python
import peerlink

try:
    client = peerlink.P2PClient(did="device-001")
    client.connect("device-002", timeout=10.0)

except peerlink.ConnectionError as e:
    print(f"Connection failed: {e}")

except peerlink.TimeoutError as e:
    print(f"Connection timeout: {e}")

except peerlink.NATDetectionError as e:
    print(f"NAT type: {e.nat_type}")

except Exception as e:
    print(f"Unexpected error: {e}")
```

## 线程安全

P2PClient类是线程安全的，可以在多线程环境中使用：

```python
import threading
import peerlink

client = peerlink.P2PClient(did="device-001")
client.connect("device-002")

def send_thread():
    for i in range(100):
        client.send(f"Message {i}".encode())

def recv_thread():
    while True:
        data = client.recv()
        print(f"Received: {data}")

t1 = threading.Thread(target=send_thread)
t2 = threading.Thread(target=recv_thread)

t1.start()
t2.start()

t1.join()
t2.join()
```

## 上下文管理器

支持上下文管理器协议：

```python
import peerlink

with peerlink.P2PClient(did="device-001") as client:
    client.connect("device-002")
    client.send(b"Hello!")
    # 自动关闭
```
