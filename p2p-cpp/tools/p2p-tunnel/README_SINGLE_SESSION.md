# Relay Tunnel - 单会话SSH隧道

## 概述

通过Relay Server建立Mac/手机到办公机的SSH隧道。单会话模式：一个连接对应一个SSH会话，连接断开后自动退出。

**核心特性：**
- ✅ 简单稳定：一对一连接，无复杂会话管理
- ✅ 已验证可用：Mac成功连接办公机，执行命令正常
- ✅ 支持手机：通过ADB端口转发或WiFi连接

## 快速开始

### 1. 启动办公机Server

```bash
# 通过跳板机SSH到办公机
sshpass -p '<PASSWORD>' ssh -J root:PASSWORD@<JUMP_HOST>:PORT <USER>@<OFFICE_SERVER_1>

# 启动server（后台运行）
cd /tmp
setsid ./relay-tunnel-single server --did office-213 --relay <YOUR_SERVER_IP>:9443 --forward 127.0.0.1:22 > tunnel-server.log 2>&1 < /dev/null &

# 查看日志确认启动
tail -f tunnel-server.log
# 看到 "Waiting for client connection..." 表示成功
```

### 2. 启动Mac Client

```bash
cd /path/to/p2p-platform/p2p-cpp/tools/p2p-tunnel

# 启动client（前台运行，或用screen/tmux）
./relay-tunnel-single client --did home-mac --target office-213 --relay <YOUR_SERVER_IP>:9443 --listen 127.0.0.1:9022 &
```

### 3. 连接办公机

```bash
# 重要：必须禁用公钥认证，只用密码认证
sshpass -p '<PASSWORD>' ssh -p 9022 \
  -o StrictHostKeyChecking=no \
  -o UserKnownHostsFile=/dev/null \
  -o PreferredAuthentications=password \
  -o PubkeyAuthentication=no \
  <USER>@127.0.0.1 'hostname; whoami; echo SUCCESS'
```

**输出示例：**
```
office-server
lhb
SUCCESS
```

## 手机连接

### 方案A：ADB端口转发（推荐）

```bash
# Mac上设置ADB reverse
adb reverse tcp:9022 tcp:9022

# 手机Termux中执行
ssh -p 9022 -o PubkeyAuthentication=no <USER>@127.0.0.1
# 密码: <PASSWORD>
```

### 方案B：WiFi直连

```bash
# Mac启动client时监听所有接口
./relay-tunnel-single client --did home-mac --target office-213 \
  --relay <YOUR_SERVER_IP>:9443 --listen 0.0.0.0:9022 &

# 手机Termux连接（假设Mac IP是192.168.3.144）
ssh -p 9022 -o PubkeyAuthentication=no <USER>@192.168.3.144
# 密码: <PASSWORD>
```

## 重要说明

### SSH认证问题

**必须禁用公钥认证！** 否则SSH会卡在密钥解锁阶段。

正确的SSH选项：
```bash
-o PreferredAuthentications=password
-o PubkeyAuthentication=no
```

### 单会话行为

- 每次SSH连接需要重新启动client和server
- 连接断开后进程自动退出
- 适合临时连接，不适合长期保持

### 调试

查看日志：
```bash
# 办公机
tail -f /tmp/tunnel-server.log

# Mac
# client输出到stdout，可重定向到文件
```

## 编译

### Mac
```bash
g++ -std=c++17 -O2 -I/opt/homebrew/include -DBOOST_ASIO_NO_DEPRECATED \
  -o relay-tunnel-single relay_tunnel_single.cpp -lpthread
```

### Linux
```bash
g++ -std=c++17 -O2 -o relay-tunnel-single relay_tunnel_single.cpp \
  -lpthread -lboost_system
```

## 测试结果

✅ **Mac → 办公机**: 成功
- hostname: office-server
- 8x NVIDIA GeForce RTX 5090
- 所有命令执行正常

✅ **手机连接**: ADB端口转发已配置
- 需要在Termux中手动测试

## 架构

```
Mac/手机 <--TCP--> relay-tunnel client <--Relay Server--> relay-tunnel server <--TCP--> 办公机SSH
         (9022)                          (<YOUR_SERVER_IP>:9443)                           (22)
```

## 故障排查

**问题：SSH连接卡住**
- 检查是否禁用了公钥认证
- 添加 `-o PubkeyAuthentication=no`

**问题：Connection refused**
- 检查client是否在运行：`lsof -i :9022`
- 重新启动client和server

**问题：Permission denied**
- 确认密码正确：<PASSWORD>
- 确认使用了sshpass或手动输入密码
