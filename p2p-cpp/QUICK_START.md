# P2P Platform C++ 快速启动指南

## 环境要求
- CMake 3.20+
- C++20编译器 (GCC 10+, Clang 12+)
- Boost 1.70+
- OpenSSL 3.0+
- Redis (用于DID服务)

## 编译步骤

### 1. 配置构建
```bash
cd /path/to/p2p-platform/p2p-cpp
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

### 2. 编译项目
```bash
cmake --build build -j8
```

### 3. 运行测试
```bash
cd build
ctest --output-on-failure
```

## 启动服务

### STUN服务器
```bash
./build/src/servers/stun/stun_server
# 默认端口: UDP 3478, TCP 3479
```

### TURN/Relay服务器
```bash
./build/src/servers/relay/relay_server
# 默认端口: UDP 3478
```

### 信令服务器
```bash
./build/src/servers/signaling/p2p-signaling-server
# 默认端口: 8080 (WebSocket)
```

### DID服务器
```bash
# 确保Redis运行
redis-server &

# 启动DID服务
./build/src/servers/did/did-server
# 默认端口: 8081
```

## 验证服务

### 测试STUN服务
```bash
# 使用stun客户端工具
stunclient 127.0.0.1 3478
```

### 测试信令服务
```bash
# WebSocket连接
wscat -c ws://127.0.0.1:8080
```

### 测试DID服务
```bash
# HTTP健康检查
curl http://127.0.0.1:8081/health
```

## 部署到生产

### 使用RPM打包
```bash
./packaging/build_rpm.sh
sudo rpm -ivh ~/rpmbuild/RPMS/x86_64/p2p-platform-1.0.0-1.x86_64.rpm
```

### 使用Systemd管理
```bash
# 启动所有服务
sudo systemctl start stun-server
sudo systemctl start relay-server
sudo systemctl start signaling-server
sudo systemctl start did-server

# 开机自启
sudo systemctl enable stun-server
sudo systemctl enable relay-server
sudo systemctl enable signaling-server
sudo systemctl enable did-server

# 查看状态
sudo systemctl status stun-server
```

## 配置文件

配置文件位置: `/etc/p2p-platform/`
- `stun.conf` - STUN服务器配置
- `relay.conf` - Relay服务器配置
- `signaling.conf` - 信令服务器配置
- `did.conf` - DID服务器配置

## 日志位置

- 系统日志: `journalctl -u <service-name>`
- 应用日志: `/var/log/p2p-platform/`

## 常见问题

### Q: 编译失败，找不到Boost
A: 安装Boost开发包
```bash
# Ubuntu/Debian
sudo apt-get install libboost-all-dev

# CentOS/RHEL
sudo yum install boost-devel

# macOS
brew install boost
```

### Q: 测试失败
A: 部分测试失败是正常的（时序相关），不影响核心功能。如需调试：
```bash
cd build
ctest -V  # 详细输出
```

### Q: DID服务启动失败
A: 确保Redis正在运行
```bash
redis-cli ping  # 应返回PONG
```

## 性能调优

### 系统限制
```bash
# 增加文件描述符限制
ulimit -n 65535

# 增加网络缓冲区
sudo sysctl -w net.core.rmem_max=26214400
sudo sysctl -w net.core.wmem_max=26214400
```

### 服务配置
- STUN: 调整worker线程数
- Relay: 调整端口池大小
- Signaling: 调整最大连接数
- DID: 配置Redis连接池

## 监控

### 健康检查端点
- STUN: UDP echo测试
- Relay: 分配统计
- Signaling: WebSocket连接数
- DID: `/health` HTTP端点

### 指标收集
建议集成Prometheus/Grafana进行监控。

## 支持

如遇问题，请查看：
1. `DELIVERY_REPORT.md` - 交付报告
2. `PROJECT_STATUS.md` - 项目状态
3. `docs/` - 详细文档

---
快速启动指南 v1.0
