# PeerLink 部署指南

## 部署架构

- **云服务端**: `<YOUR_SERVER_IP>` (`<YOUR_USER>`)
- **办公室客户端**: 
  - 服务器A: `<USER>@<OFFICE_SERVER_1>`
  - 服务器B: `<USER>@<OFFICE_SERVER_2>`
- **本地客户端**: 
  - 本机MacBook (已编译)
  - 家里MacBook Pro (待部署)

## 快速部署步骤

### 1. 阿里云服务端部署

由于架构差异（本地arm64 vs 服务器x86_64），需要在服务器上编译。

**方式A: 使用部署脚本（推荐）**
```bash
# 在本地执行
./deploy_remote.sh <user>@<YOUR_SERVER_IP> server
```

**方式B: 手动部署**
```bash
# 1. 创建源码包
tar czf /tmp/peerlink-src.tar.gz \
    --exclude='.git' --exclude='build*' --exclude='*.o' \
    CMakeLists.txt src/ include/ proto/ examples/ config/ deploy/ third_party/ tests/

# 2. 上传到服务器
scp /tmp/peerlink-src.tar.gz <user>@<YOUR_SERVER_IP>:/tmp/

# 3. SSH登录服务器
ssh <user>@<YOUR_SERVER_IP>

# 4. 在服务器上编译
cd /tmp
tar xzf peerlink-src.tar.gz
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_SERVERS=ON -DBUILD_SHARED_LIBS=OFF
cmake --build build --target stun_server relay_server p2p-signaling-server did-server -j$(nproc)

# 5. 安装二进制文件
sudo mkdir -p /opt/peerlink/bin /etc/peerlink
sudo cp build/src/servers/stun/stun_server /opt/peerlink/bin/
sudo cp build/src/servers/relay/relay_server /opt/peerlink/bin/
sudo cp build/src/servers/signaling/p2p-signaling-server /opt/peerlink/bin/
sudo cp build/src/servers/did/did-server /opt/peerlink/bin/

# 6. 配置文件
sudo cp config/peerlink.aliyun.example.toml /etc/peerlink/peerlink.toml
sudo vi /etc/peerlink/peerlink.toml
# 修改以下配置:
#   - jwt_secret: 替换为随机密钥
#   - public_ip: 设置为 <YOUR_PUBLIC_IP>

# 7. 安装systemd服务
sudo cp deploy/systemd/*.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable peerlink-stun peerlink-relay peerlink-signaling peerlink-did
sudo systemctl start peerlink-stun peerlink-relay peerlink-signaling peerlink-did

# 8. 检查服务状态
sudo systemctl status peerlink-*
```

### 2. 办公室客户端部署（213、215）

**前提条件**: 需要在办公室网络或通过VPN连接

```bash
# 在本地执行
./deploy_remote.sh <user>@<OFFICE_SERVER_1> client
./deploy_remote.sh <user>@<OFFICE_SERVER_2> client
```

部署完成后，在服务器上测试：
```bash
ssh <user>@<OFFICE_SERVER_1>
cd /tmp/peerlink-deploy-*/
./build/examples/basic/basic_client <my_did> <peer_did>
```

### 3. 本地MacBook客户端

本地已编译完成（arm64架构），但需要注意：
- 服务器组件是arm64，只能在本机测试
- 要连接到云服务端，需要配置客户端指向 `<YOUR_SERVER_IP>`

**测试本地客户端**:
```bash
# 编译客户端示例（如果还没编译）
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=ON
cmake --build build --target basic_client

# 运行客户端（需要先启动阿里云服务端）
./build/examples/basic/basic_client <my_did> <peer_did>
```

### 4. 家里MacBook Pro部署

与本地MacBook相同的步骤：
```bash
# 1. 克隆代码
git clone <repo_url>
cd p2p-platform/p2p-cpp

# 2. 编译
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=ON
cmake --build build --target basic_client

# 3. 运行客户端
./build/examples/basic/basic_client <my_did> <peer_did>
```

## 互通测试

### 测试场景

1. **客户端到服务端连接测试**
   - 客户端连接到信令服务器 (`<YOUR_SERVER_IP>:8080`)
   - 客户端通过STUN获取公网地址 (`<YOUR_SERVER_IP>:3478`)

2. **客户端之间P2P连接测试**
   - 213客户端 <-> 215客户端
   - 本机MacBook <-> 办公室客户端A
   - 本机MacBook <-> 办公室客户端B
   - 家里MacBook <-> 办公室客户端

3. **NAT穿透测试**
   - 测试不同NAT类型下的穿透成功率
   - 测试Relay fallback机制

### 测试命令

```bash
# 在客户端A上
./basic_client did:peer:alice did:peer:bob

# 在客户端B上
./basic_client did:peer:bob did:peer:alice
```

### 调试工具

1. **检查服务端状态**
```bash
# 在阿里云服务器上
sudo systemctl status peerlink-*
sudo journalctl -u peerlink-signaling -f
sudo journalctl -u peerlink-stun -f
```

2. **检查端口监听**
```bash
sudo netstat -tulpn | grep -E '8080|3478|9001'
```

3. **测试端口连通性**
```bash
# 从客户端测试
nc -zv <YOUR_SERVER_IP> 8080  # 信令服务器
nc -zuv <YOUR_SERVER_IP> 3478 # STUN服务器
```

## 常见问题

### 1. SSH连接超时
- **原因**: 不在办公室网络或VPN未连接
- **解决**: 连接VPN或在办公室网络执行部署

### 2. 编译失败
- **原因**: 缺少依赖库
- **解决**: 
```bash
# Ubuntu/Debian
sudo apt-get install -y build-essential cmake libboost-all-dev libssl-dev \
  protobuf-compiler libprotobuf-dev nlohmann-json3-dev

# macOS
brew install cmake boost openssl protobuf nlohmann-json
```

### 3. 服务启动失败
- **原因**: 端口被占用或配置错误
- **解决**: 
```bash
# 检查端口占用
sudo lsof -i :8080
sudo lsof -i :3478

# 检查配置文件
sudo cat /etc/peerlink/peerlink.toml

# 查看日志
sudo journalctl -u peerlink-signaling -n 50
```

### 4. 客户端连接失败
- **原因**: 服务端未启动或防火墙阻止
- **解决**:
```bash
# 检查服务端状态
ssh <user>@<YOUR_SERVER_IP> "systemctl status peerlink-*"

# 检查防火墙规则
ssh <user>@<YOUR_SERVER_IP> "sudo iptables -L -n | grep -E '8080|3478'"
```

## 配置文件说明

### 服务端配置 (/etc/peerlink/peerlink.toml)

关键配置项：
```toml
[server]
public_ip = "<YOUR_PUBLIC_IP>"  # 云服务器公网IP
jwt_secret = "your-random-secret-here"  # 必须修改

[signaling]
host = "0.0.0.0"
port = 8080
allow_insecure_registration = false  # 生产环境必须为false

[stun]
host = "0.0.0.0"
port = 3478

[relay]
host = "0.0.0.0"
port = 9001
min_port = 50000
max_port = 50100
```

### 客户端配置

在代码中配置：
```cpp
core::P2PConfig config;
config.signaling_server = "<YOUR_SERVER_IP>";
config.signaling_port = 8080;
config.stun_server = "<YOUR_SERVER_IP>";
config.stun_port = 3478;
```

## 下一步

1. ✅ 本地编译完成
2. ⏳ 阿里云服务端部署（需要手动执行）
3. ⏳ 办公室客户端部署（需要VPN或办公室网络）
4. ⏳ 互通测试
5. ⏳ 问题调试和优化

## 联系方式

如遇问题，请检查：
1. 服务端日志: `sudo journalctl -u peerlink-* -f`
2. 网络连通性: `nc -zv <YOUR_SERVER_IP> 8080`
3. 配置文件: `/etc/peerlink/peerlink.toml`
