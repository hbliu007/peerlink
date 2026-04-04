# Quick Start Guide - Multi-Session Relay Tunnel

## 当前状态

✅ **MacBook (家里)** - 已编译完成
- 位置: `/path/to/p2p-platform/p2p-cpp/tools/p2p-tunnel/relay-tunnel`
- 大小: 584KB
- 状态: 可直接使用

⏳ **办公室电脑** - 待部署
- 需要复制源码并重新编译
- 使用 `deploy-to-office.sh` 自动部署

## 快速启动步骤

### 步骤1: 部署到办公室电脑

**选项A - 自动部署（推荐）**
```bash
cd /path/to/p2p-platform/p2p-cpp/tools/p2p-tunnel
./deploy-to-office.sh
```

**选项B - 手动部署**
```bash
# 1. 复制文件
scp relay_tunnel.cpp build.sh <USER>@<OFFICE_SERVER_1>:~/p2p-tunnel/

# 2. SSH到办公室电脑
ssh <USER>@<OFFICE_SERVER_1>

# 3. 编译
cd ~/p2p-tunnel
chmod +x build.sh
./build.sh
```

### 步骤2: 启动服务器端（办公室电脑）

```bash
cd ~/p2p-tunnel
./relay-tunnel server --did office-213 --relay <YOUR_SERVER_IP>:9443 --forward 127.0.0.1:22
```

**预期输出**：
```
=== Relay Tunnel Server ===
DID: office-213
Relay: <YOUR_SERVER_IP>:9443
Forward: 127.0.0.1:22
[Server] Starting multi-session server...
[Server] Session #1 - Creating relay connection with DID: office-213-session-1
[Relay] Connected to <YOUR_SERVER_IP>:9443
[Relay] Registered as: office-213-session-1
[Server] Session #1 - Waiting for client connection...
```

### 步骤3: 启动客户端（MacBook）

```bash
cd /path/to/p2p-platform/p2p-cpp/tools/p2p-tunnel
./relay-tunnel client --did home-mac --target office-213 --relay <YOUR_SERVER_IP>:9443 --listen 9022
```

**预期输出**：
```
=== Relay Tunnel Client ===
DID: home-mac
Target: office-213
Relay: <YOUR_SERVER_IP>:9443
Listen: 127.0.0.1:9022
[Client] Listening on 127.0.0.1:9022
[Client] Tunnel ready! Use: ssh -p 9022 <USER>@127.0.0.1
```

### 步骤4: 测试多会话功能

**自动测试**：
```bash
./test-multi-session.sh
```

**手动测试**：
```bash
# 终端1
ssh -p 9022 <USER>@127.0.0.1

# 终端2（同时打开）
ssh -p 9022 <USER>@127.0.0.1

# 终端3（同时打开）
ssh -p 9022 <USER>@127.0.0.1
```

## 验证成功标志

### 客户端日志
看到多个会话创建：
```
[Client] Session #1 - TCP connection accepted
[Client] Session #1 - Tunnel established, relaying data...
[Client] Session #2 - TCP connection accepted
[Client] Session #2 - Tunnel established, relaying data...
```

### 服务器端日志
看到多个会话处理：
```
[Server] Session #1 - Client connected: home-mac-session-1
[Server] Session #1 - Tunnel active! Relaying data...
[Server] Session #2 - Client connected: home-mac-session-2
[Server] Session #2 - Tunnel active! Relaying data...
```

### SSH连接
- 可以同时打开多个SSH会话
- 断开一个会话不影响其他会话
- 断开后可以立即重新连接，无需重启tunnel

## 故障排除

### 问题1: 办公室电脑无法连接
```bash
# 检查网络
ping <OFFICE_SERVER_1>

# 检查SSH
ssh -v <USER>@<OFFICE_SERVER_1>
```

**解决方案**：
- 确保在办公室网络或VPN
- 确保办公室电脑已开机
- 手动复制文件到办公室电脑

### 问题2: 编译失败
```bash
# Linux系统需要安装boost
sudo apt-get install libboost-all-dev

# 或使用系统包管理器
sudo yum install boost-devel
```

### 问题3: 连接超时
- 检查 relay 是否可达（明文协议，非 HTTPS）：`printf 'REGISTER test\n' | nc -w 2 <YOUR_SERVER_IP> 9443`
- 检查防火墙设置
- 确认DID配置正确

### 问题4: 第二个会话无法连接
- 检查客户端日志是否显示 "Session #2"
- 检查服务器端日志是否显示 "Session #2"
- 如果没有，说明多会话功能未生效，需要重新编译

## 性能说明

- **延迟**: 取决于relay服务器位置和网络质量
- **带宽**: 受relay服务器带宽限制
- **并发**: 理论上无限制，实际受系统资源限制
- **稳定性**: 每个会话独立，互不影响

## 文件清单

```
relay_tunnel.cpp          - 主程序源码（支持多会话）
relay-tunnel              - MacBook编译好的可执行文件
build.sh                  - 跨平台编译脚本
deploy-to-office.sh       - 自动部署脚本
test-multi-session.sh     - 多会话测试脚本
MULTI_SESSION_README.md   - 详细使用文档
CHANGES.md                - 修改详情和技术说明
QUICKSTART.md             - 本文件
```

## 下一步

1. ✅ 完成MacBook编译
2. ⏳ 部署到办公室电脑
3. ⏳ 启动服务器端
4. ⏳ 启动客户端
5. ⏳ 测试多会话功能
6. ⏳ 验证长时间稳定性

## 联系方式

如有问题，请查看：
- `MULTI_SESSION_README.md` - 详细文档
- `CHANGES.md` - 技术细节
- 或联系开发者
