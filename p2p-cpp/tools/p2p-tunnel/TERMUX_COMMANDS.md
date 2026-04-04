# 手机端部署指令（在Termux中执行）

请在手机Termux中依次执行以下命令：

## 1. 复制源代码到Termux主目录
```bash
cp /sdcard/Download/p2p-cpp-minimal.tar.gz ~/
cd ~
tar xzf p2p-cpp-minimal.tar.gz
ls -lh p2p-cpp-minimal.tar.gz
```

## 2. 安装编译依赖
```bash
pkg update
pkg install -y cmake clang boost openssl
```

## 3. 编译P2P隧道客户端
```bash
cd ~/p2p-cpp
mkdir -p build
cd build
cmake .. -DBUILD_TESTS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_SERVERS=OFF
make p2p-tunnel-client -j4
```

## 4. 检查编译结果
```bash
ls -lh tools/p2p-tunnel/p2p-tunnel-client
file tools/p2p-tunnel/p2p-tunnel-client
```

## 5. 启动P2P隧道客户端
```bash
cd tools/p2p-tunnel
./p2p-tunnel-client mobile-001 office-001 9000 8080 ws://<YOUR_SERVER_IP>:8080 <YOUR_SERVER_IP>:3478
```

---

## 预期输出

启动后应该看到类似日志：
```
P2P Tunnel Client starting...
Device ID: mobile-001
Server ID: office-001
Local port: 9000
Remote port: 8080
Signaling: ws://<YOUR_SERVER_IP>:8080
STUN: <YOUR_SERVER_IP>:3478
Initializing P2P client...
NAT type detected: [NAT类型]
Connecting to server...
```

## 故障排查

### 如果编译失败
- 检查存储空间：`df -h`
- 更新Termux：`pkg upgrade`
- 重新安装依赖

### 如果连接失败
- 检查网络：`ping <YOUR_SERVER_IP>`
- 检查办公室服务端是否运行
- 查看NAT类型检测结果

## 后台运行（可选）

如果需要后台运行，使用tmux：
```bash
pkg install tmux
tmux new -s p2p
# 在tmux中启动客户端
# 按 Ctrl+B 然后按 D 分离会话
# 重新连接：tmux attach -t p2p
```
