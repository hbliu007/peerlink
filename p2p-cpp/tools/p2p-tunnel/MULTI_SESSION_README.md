# Relay Tunnel - Multi-Session Support

## 修改内容

### 问题
原版本只支持单个SSH连接，连接断开后需要重启tunnel才能再次使用。

### 解决方案
实现了完整的多会话支持：

1. **客户端模式**：
   - 持续监听本地端口（9022）
   - 每个新的TCP连接创建独立的relay会话
   - 自动生成唯一会话DID（home-mac-session-1, home-mac-session-2, ...）
   - 会话结束后自动清理，但保持监听

2. **服务器模式**：
   - 持续接受新的relay连接
   - 每个连接转发到本地SSH服务
   - 自动生成唯一会话DID（office-213-session-1, office-213-session-2, ...）
   - 支持多个并发SSH会话

3. **关键改进**：
   - 移除了`io.stop()`调用，不再在连接断开时终止程序
   - 使用递归accept模式持续接受新连接
   - 每个会话独立管理，互不影响
   - 会话断开时自动清理资源

## 编译

### macOS
```bash
./build.sh
```

### Linux (办公室电脑)
```bash
./build.sh
```

或手动编译：
```bash
g++ -std=c++17 -O2 -o relay-tunnel relay_tunnel.cpp -lpthread -lboost_system
```

## 使用方法

### 1. 启动服务器端（办公室电脑）
```bash
./relay-tunnel server --did office-213 --relay <YOUR_SERVER_IP>:9443 --forward 127.0.0.1:22
```

### 2. 启动客户端（家里MacBook）
```bash
./relay-tunnel client --did home-mac --target office-213 --relay <YOUR_SERVER_IP>:9443 --listen 9022
```

### 3. 使用SSH连接（支持多个并发会话）
```bash
# 会话1
ssh -p 9022 <USER>@127.0.0.1

# 会话2（在另一个终端）
ssh -p 9022 <USER>@127.0.0.1

# 会话3（在另一个终端）
ssh -p 9022 <USER>@127.0.0.1
```

## 测试

运行自动化测试脚本：
```bash
./test-multi-session.sh
```

该脚本会同时打开3个SSH会话，验证多会话支持是否正常工作。

## 日志输出

程序会显示每个会话的详细日志：

**客户端**：
```
[Client] Session #1 - TCP connection accepted
[Client] Creating new relay connection with DID: home-mac-session-1
[Client] Session #1 - Tunnel established, relaying data...
[Client] Session #2 - TCP connection accepted
[Client] Creating new relay connection with DID: home-mac-session-2
...
```

**服务器端**：
```
[Server] Session #1 - Creating relay connection with DID: office-213-session-1
[Server] Session #1 - Client connected: home-mac-session-1
[Server] Session #1 - Tunnel active! Relaying data...
[Server] Session #2 - Creating relay connection with DID: office-213-session-2
...
```

## 架构说明

```
客户端（家里）:
  监听 127.0.0.1:9022
    ↓
  每个新TCP连接 → 创建新RelayConnection
    ↓
  REGISTER home-mac-session-N
    ↓
  CONNECT office-213
    ↓
  数据双向转发

服务器端（办公室）:
  持续接受relay连接
    ↓
  每个连接 REGISTER office-213-session-N
    ↓
  等待客户端CONNECT
    ↓
  转发到本地SSH (127.0.0.1:22)
    ↓
  数据双向转发
```

## 部署步骤

1. **在MacBook上**：
   - 已编译完成：`/path/to/p2p-platform/p2p-cpp/tools/p2p-tunnel/relay-tunnel`
   - 启动客户端即可使用

2. **在办公室电脑上**：
   - 复制源码：`relay_tunnel.cpp` 和 `build.sh`
   - 运行：`./build.sh`
   - 启动服务器端

3. **验证**：
   - 运行 `./test-multi-session.sh` 测试多会话功能
   - 或手动打开多个SSH会话验证
