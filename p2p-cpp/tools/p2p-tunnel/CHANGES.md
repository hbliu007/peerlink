# Relay Tunnel Multi-Session 修改总结

## 修改日期
2026-04-03

## 问题描述
原relay-tunnel只支持单个SSH连接：
- 客户端accept一次连接后停止监听
- 连接断开时调用`io.stop()`终止程序
- 需要手动重启tunnel才能建立新连接

## 解决方案

### 1. 客户端模式 (run_client)
**修改前**：
```cpp
// 先注册 → 连接target → 然后accept一次 → 结束
relay->connect_and_register(did, ...);
  relay->connect_to_peer(target_did, ...);
    acceptor->async_accept(...); // 只accept一次
      // 连接断开时 io.stop()
```

**修改后**：
```cpp
// 先启动acceptor循环 → 每个连接创建新relay
auto acceptor = std::make_shared<tcp::acceptor>(...);
std::function<void()> do_accept;
do_accept = [...]() {
    acceptor->async_accept([...](...) {
        // 为每个连接创建新的RelayConnection
        auto relay = std::make_shared<RelayConnection>(...);
        relay->connect_and_register(session_did, ...);
          relay->connect_to_peer(target_did, ...);
            // 建立tunnel
        
        // 继续accept下一个连接
        do_accept();
    });
};
do_accept(); // 启动循环
```

**关键改进**：
- 使用递归lambda实现持续accept
- 每个TCP连接创建独立的RelayConnection
- 生成唯一会话DID：`home-mac-session-1`, `home-mac-session-2`, ...
- 移除`io.stop()`，会话结束只清理当前会话资源

### 2. 服务器模式 (run_server)
**修改前**：
```cpp
// 创建一个relay → 注册 → 等待连接 → 转发 → 结束
auto relay = std::make_shared<RelayConnection>(...);
relay->connect_and_register(did, ...);
  relay->wait_for_connection(...);
    // 连接到本地SSH
    // 连接断开时 io.stop()
```

**修改后**（2026-04-03 DID匹配修复）：
```cpp
// 递归创建多个relay连接，使用固定的base_did
std::function<void()> create_session;
create_session = [...]() {
    auto relay = std::make_shared<RelayConnection>(...);
    relay->connect_and_register(base_did, ...);  // 使用固定DID，如office-213
      relay->wait_for_connection(...);
        // 连接到本地SSH
        // 建立tunnel

        // 会话结束后创建下一个会话
        create_session();
};
create_session(); // 启动循环
```

**关键改进**：
- 使用递归lambda持续接受新relay连接
- 每个会话使用**固定的** base_did（如 `office-213`）注册
- 客户端可以始终用固定的 target_did（如 `office-213`）连接
- 移除`io.stop()`，会话结束只清理当前会话资源
- 会话结束后自动重新注册，等待下一个连接

### 3. RelayConnection类
**新增方法**：
```cpp
asio::io_context& get_io_context() {
    return io_;
}
```
用于服务器端创建本地socket时获取io_context引用。

## 代码统计

### 修改的文件
- `relay_tunnel.cpp` (主要修改)

### 新增的文件
- `build.sh` - 跨平台编译脚本
- `test-multi-session.sh` - 多会话测试脚本
- `deploy-to-office.sh` - 部署到办公室电脑脚本
- `MULTI_SESSION_README.md` - 使用文档
- `CHANGES.md` - 本文件

### 代码行数变化
- 客户端函数：~50行 → ~80行
- 服务器端函数：~60行 → ~100行
- 总增加：约70行

## 架构对比

### 原架构
```
Client: 监听端口 → accept一次 → 使用已有relay → 断开后退出
Server: 创建relay → 等待一次 → 转发 → 断开后退出
```

### 新架构
```
Client: 监听端口 → 循环accept → 每次创建新relay → 会话独立
Server: 循环创建relay → 每个等待连接 → 转发 → 会话独立
```

## 测试验证

### 测试场景
1. 单个SSH会话 - 基本功能
2. 多个并发SSH会话 - 多会话支持
3. 会话断开重连 - 持续监听
4. 长时间运行 - 稳定性

### 测试命令
```bash
# 自动化测试
./test-multi-session.sh

# 手动测试
# 终端1
ssh -p 9022 <USER>@127.0.0.1

# 终端2
ssh -p 9022 <USER>@127.0.0.1

# 终端3
ssh -p 9022 <USER>@127.0.0.1
```

## 部署步骤

### MacBook (已完成)
```bash
cd /path/to/p2p-platform/p2p-cpp/tools/p2p-tunnel
./build.sh
# 生成: relay-tunnel (584KB)
```

### 办公室电脑 (待部署)
```bash
# 方式1: 自动部署（需要网络连接）
./deploy-to-office.sh

# 方式2: 手动部署
# 1. 复制文件到办公室电脑
scp relay_tunnel.cpp build.sh <USER>@<OFFICE_SERVER_1>:~/p2p-tunnel/

# 2. SSH到办公室电脑
ssh <USER>@<OFFICE_SERVER_1>

# 3. 编译
cd ~/p2p-tunnel
./build.sh
```

## 使用说明

### 启动服务器（办公室）
```bash
./relay-tunnel server --did office-213 --relay <YOUR_SERVER_IP>:443 --forward 127.0.0.1:22
```

### 启动客户端（家里）
```bash
./relay-tunnel client --did home-mac --target office-213 --relay <YOUR_SERVER_IP>:443 --listen 9022
```

### 使用SSH
```bash
ssh -p 9022 <USER>@127.0.0.1
# 可以打开多个终端，每个都能独立连接
```

## 日志示例

### 客户端日志
```
[Client] Listening on 127.0.0.1:9022
[Client] Tunnel ready! Use: ssh -p 9022 <USER>@127.0.0.1

[Client] Session #1 - TCP connection accepted
[Client] Creating new relay connection with DID: home-mac-session-1
[Relay] Connected to <YOUR_SERVER_IP>:443
[Relay] Registered as: home-mac-session-1
[Client] Session #1 - Tunnel established, relaying data...

[Client] Session #2 - TCP connection accepted
[Client] Creating new relay connection with DID: home-mac-session-2
[Relay] Connected to <YOUR_SERVER_IP>:443
[Relay] Registered as: home-mac-session-2
[Client] Session #2 - Tunnel established, relaying data...
```

### 服务器端日志
```
[Server] Starting multi-session server...

[Server] Session #1 - Creating relay connection with DID: office-213-session-1
[Relay] Connected to <YOUR_SERVER_IP>:443
[Relay] Registered as: office-213-session-1
[Server] Session #1 - Waiting for client connection...
[Server] Session #1 - Client connected: home-mac-session-1
[Server] Session #1 - Connected to local service at 127.0.0.1:22
[Server] Session #1 - Tunnel active! Relaying data...

[Server] Session #2 - Creating relay connection with DID: office-213-session-2
[Relay] Connected to <YOUR_SERVER_IP>:443
[Relay] Registered as: office-213-session-2
[Server] Session #2 - Waiting for client connection...
```

## 注意事项

1. **会话DID唯一性**：每个会话使用递增计数器生成唯一DID
2. **资源清理**：会话结束时自动清理socket和relay连接
3. **错误处理**：单个会话失败不影响其他会话和后续连接
4. **信号处理**：Ctrl+C仍然会优雅退出整个程序
5. **并发限制**：理论上无限制，实际受系统资源限制

## 后续优化建议

1. **会话管理**：添加会话列表和状态查询
2. **资源限制**：限制最大并发会话数
3. **会话超时**：自动清理长时间空闲会话
4. **统计信息**：记录会话数量、流量等
5. **日志级别**：添加可配置的日志详细程度
