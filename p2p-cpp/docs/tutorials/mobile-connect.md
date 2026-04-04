---
title: 手机连接教程
description: 从 Android (Termux) 或 iOS 设备通过 Relay 隧道 SSH 连接办公机
tags:
  - 教程
  - 手机连接
  - Termux
  - Android
  - iOS
  - SSH
---

# 手机连接教程

本教程展示如何从 Android 或 iOS 手机通过 Relay 隧道 SSH 连接到办公机。

## 架构概览

```
手机（移动网络）──TCP 443──→ 阿里云 Relay ←──TCP 443── 办公机
                     [SSH 数据双向流动]
```

手机使用自身网络（4G/5G/WiFi），不依赖笔记本或中间设备。

---

## Android: Termux 连接

### 步骤 1: 安装 Termux

!!! warning "不要使用 Google Play 版本"
    Google Play 上的 Termux 已过时且不再更新。请从以下渠道下载：

    - **F-Droid**: https://f-droid.org/packages/com.termux/
    - **GitHub Releases**: https://github.com/termux/termux-app/releases

### 步骤 2: 配置 Termux 环境

```bash
# 更新包管理器（首次运行较慢，耐心等待）
pkg update -y && pkg upgrade -y

# 安装 SSH 客户端
pkg install -y openssh

# 授予存储权限（弹出权限请求，点击允许）
termux-setup-storage
```

### 步骤 3: 获取 relay-tunnel 二进制

从电脑传输到手机：

```bash
# 1. 通过 adb 推送到手机
adb push relay-tunnel /sdcard/Download/relay-tunnel-single

# 2. 在 Termux 中复制到 home 目录
cp /sdcard/Download/relay-tunnel-single ~/
chmod +x ~/relay-tunnel-single

# 3. 验证文件
ls -lh ~/relay-tunnel-single
```

### 步骤 4: 启动 Tunnel Client

```bash
# 启动 tunnel（保持此窗口运行）
~/relay-tunnel-single client \
  --did phone-android \
  --target office-213 \
  --relay <YOUR_SERVER_IP>:443 \
  --listen 127.0.0.1:9022
```

成功输出：

```
=== Relay Tunnel Client ===
DID: phone-android
Target: office-213
Relay: <YOUR_SERVER_IP>:443
Listen: 127.0.0.1:9022
[Client] Listening on 127.0.0.1:9022
[Client] Tunnel ready! Use: ssh -p 9022 <USER>@127.0.0.1
```

### 步骤 5: SSH 连接

打开 Termux **新会话**（向左滑动，点击 "NEW SESSION"）：

```bash
# 测试连接
ssh -p 9022 -o PubkeyAuthentication=no <USER>@127.0.0.1

# 一行命令验证
ssh -p 9022 -o PubkeyAuthentication=no <USER>@127.0.0.1 'hostname; whoami; echo SUCCESS'
```

预期输出：

```
office-server
lhb
SUCCESS
```

---

## iOS: 通过 USB 共享笔记本隧道

iOS 没有 Termux 等终端模拟器，需要通过 USB 连接笔记本共享隧道。

### 方式 1: 使用 USB 端口转发

笔记本上已运行 relay-tunnel client 后，利用 iOS SSH 客户端应用（如 Termius、Blink Shell）连接笔记本转发的端口。

```bash
# 笔记本上：确保 tunnel 监听所有接口
./relay-tunnel client \
  --did home-mac \
  --target office-213 \
  --relay <YOUR_SERVER_IP>:9443 \
  --listen 0.0.0.0:9022
```

然后在 iOS SSH 应用中：

| 字段 | 值 |
|------|---|
| Host | 笔记本 IP（如 `192.168.1.100`） |
| Port | `9022` |
| Username | `lhb` |
| Password | 办公机密码 |

### 方式 2: 个人热点回连

如果笔记本通过 iPhone 热点联网，笔记本的 IP 通常为 `172.20.10.x`：

```bash
# iOS SSH 应用中连接
Host: 172.20.10.1
Port: 9022
```

---

## 笔记本共享方式

### ADB 端口转发（Android USB 连接）

当手机通过 USB 连接笔记本时：

```bash
# 笔记本上执行（手机 USB 连接状态）
adb forward tcp:9022 tcp:9022

# 手机 Termux 中
ssh -p 9022 -o PubkeyAuthentication=no <USER>@127.0.0.1
```

数据路径：

```
Termux SSH → USB adb forward → 笔记本:9022 → relay-tunnel → 阿里云 → 办公机
```

### WiFi 直连方式

```bash
# 笔记本上：监听所有接口
./relay-tunnel client \
  --did home-mac \
  --target office-213 \
  --relay <YOUR_SERVER_IP>:9443 \
  --listen 0.0.0.0:9022

# 手机上（假设笔记本 IP 是 192.168.1.100）
ssh -p 9022 -o PubkeyAuthentication=no <USER>@192.168.1.100
```

!!! warning "WiFi 直连安全提示"
    `0.0.0.0` 监听对所有网络接口开放。建议仅在可信家庭 WiFi 下使用，避免在公共 WiFi 中开启。

---

## Termux 便利设置

### 创建启动脚本

```bash
cat > ~/start-tunnel.sh << 'EOF'
#!/data/data/com.termux/files/usr/bin/bash
echo "Starting relay tunnel..."
~/relay-tunnel-single client \
  --did phone-android \
  --target office-213 \
  --relay <YOUR_SERVER_IP>:443 \
  --listen 127.0.0.1:9022
EOF

chmod +x ~/start-tunnel.sh
```

以后只需执行 `~/start-tunnel.sh`。

### 快速 SSH 别名

```bash
echo "alias office='ssh -p 9022 -o PubkeyAuthentication=no <USER>@127.0.0.1'" >> ~/.bashrc
source ~/.bashrc

# 以后只需输入
office
```

### 后台运行（tmux）

```bash
# 安装 tmux
pkg install -y tmux

# 在 tmux 中启动 tunnel
tmux new -s tunnel
~/start-tunnel.sh

# Ctrl+B 然后按 D 退出 tmux（tunnel 继续运行）
# 重新连接
tmux attach -t tunnel
```

### 保持屏幕常亮

```bash
# 防止手机息屏导致 tunnel 断开
termux-wake-lock
```

### 开机自启（高级）

安装 Termux:Boot 插件（从 F-Droid），然后：

```bash
mkdir -p ~/.termux/boot
cat > ~/.termux/boot/start-tunnel.sh << 'EOF'
#!/data/data/com.termux/files/usr/bin/bash
termux-wake-lock
~/relay-tunnel-single client \
  --did phone-android \
  --target office-213 \
  --relay <YOUR_SERVER_IP>:443 \
  --listen 127.0.0.1:9022 \
  > ~/tunnel.log 2>&1 &
EOF
chmod +x ~/.termux/boot/start-tunnel.sh
```

---

## 常见问题

### Q: `pkg` 命令找不到

确保使用 F-Droid 或 GitHub 版本的 Termux，不是 Google Play 版本。

### Q: SSH 连接被拒绝

```bash
# 检查 tunnel 是否运行
ps aux | grep relay-tunnel

# 检查端口是否监听
netstat -tlnp | grep 9022
```

### Q: SSH 连接卡住

必须添加 `-o PubkeyAuthentication=no`。SSH 客户端默认尝试公钥认证，在隧道环境中可能导致卡住。

### Q: 办公机 tunnel server 没运行

需要先通过其他方式（如跳板机）远程启动办公机端的 tunnel：

```bash
sshpass -p '<PASSWORD>' ssh -J root@jump-host:port <USER>@<OFFICE_SERVER_1> \
  'nohup /tmp/relay-tunnel-single server --did office-213 \
   --relay <YOUR_SERVER_IP>:443 --forward 127.0.0.1:22 \
   > /tmp/relay.log 2>&1 &'
```

### Q: 手机网络切换后断开

切换 WiFi/移动数据后，tunnel 会断开。重新运行 `~/start-tunnel.sh` 即可恢复。

---

## 下一步

- [SSH 隧道教程](ssh-tunnel.md) - 笔记本端完整配置
- [Python SDK 快速入门](python-sdk.md) - 用代码建立 P2P 连接
- [防火墙穿透设计](../concepts/architecture.md) - 了解底层原理
