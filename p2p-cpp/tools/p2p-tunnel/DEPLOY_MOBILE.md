# 手机端部署指南 (Termux)

## 前提条件

- 华为Mate50 Pro已通过USB连接到Mac
- 手机已安装Termux
- 手机已安装Claude Code

## 部署步骤

### 1. 传输源代码到手机

```bash
# 在Mac上执行
cd /path/to/p2p-platform/p2p-cpp
adb push p2p-cpp-minimal.tar.gz /sdcard/Download/

# 在手机Termux中执行
cp /sdcard/Download/p2p-cpp-minimal.tar.gz ~/
cd ~
tar xzf p2p-cpp-minimal.tar.gz
```

### 2. 安装编译依赖

```bash
# 在Termux中执行
pkg update
pkg install cmake clang boost openssl git
```

### 3. 编译客户端

```bash
cd ~/p2p-cpp
mkdir -p build
cd build
cmake .. -DBUILD_TESTS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_SERVERS=OFF
make p2p-tunnel-client -j4
```

### 4. 启动客户端

```bash
cd tools/p2p-tunnel
./p2p-tunnel-client mobile-001 office-001 9000 8080 \
  ws://<YOUR_SERVER_IP>:8080 <YOUR_SERVER_IP>:3478
```

参数说明：
- `mobile-001`: 手机设备ID
- `office-001`: 办公室电脑设备ID
- `9000`: 本地监听端口
- `8080`: 远程目标端口
- `ws://<YOUR_SERVER_IP>:8080`: Signaling服务器
- `<YOUR_SERVER_IP>:3478`: STUN服务器

### 5. 配置Claude Code

在手机Claude Code中配置连接到本地代理：

```bash
# 方式1：环境变量
export CLAUDE_CODE_SERVER=127.0.0.1:9000
claude-code connect

# 方式2：命令行参数
claude-code connect --server 127.0.0.1:9000
```

## 故障排查

### 编译失败

1. 检查Termux版本：`termux-info`
2. 更新所有包：`pkg upgrade`
3. 检查存储空间：`df -h`

### 运行时错误

1. 检查动态库：`ldd p2p-tunnel-client`
2. 检查端口占用：`netstat -tuln | grep 9000`
3. 查看日志输出

### 连接失败

1. 检查网络连接：`ping <YOUR_SERVER_IP>`
2. 检查办公室服务端是否运行
3. 检查NAT类型检测结果

## 后台运行

使用tmux或screen在后台运行：

```bash
# 安装tmux
pkg install tmux

# 启动tmux会话
tmux new -s p2p-tunnel

# 运行客户端
./p2p-tunnel-client mobile-001 office-001 9000 8080 \
  ws://<YOUR_SERVER_IP>:8080 <YOUR_SERVER_IP>:3478

# 分离会话：Ctrl+B, D
# 重新连接：tmux attach -t p2p-tunnel
```

## 自动启动

创建启动脚本：

```bash
cat > ~/start-p2p-tunnel.sh << 'EOF'
#!/data/data/com.termux/files/usr/bin/bash
cd ~/p2p-cpp/build/tools/p2p-tunnel
./p2p-tunnel-client mobile-001 office-001 9000 8080 \
  ws://<YOUR_SERVER_IP>:8080 <YOUR_SERVER_IP>:3478
EOF

chmod +x ~/start-p2p-tunnel.sh
```

使用Termux:Boot自动启动（需要安装Termux:Boot应用）。
