---
title: 版本变更日志
description: PeerLink版本历史和重要变更记录
tags:
  - changelog
  - versions
  - history
---

# 版本变更日志

## v1.0.0 (2024-01-15)

### 重大特性

#### 完整的P2P SDK
- C++核心库实现
- C API绑定
- Python SDK
- 多平台支持（Linux, macOS）

#### 服务器组件
- **Signaling Server**: WebSocket信令服务器，支持设备注册和信令交换
- **STUN Server**: RFC 5389兼容的STUN服务器，NAT类型检测
- **TURN Server**: TURN中继服务器，支持UDP打洞失败时的数据中继
- **DID Server**: 去中心化标识服务器，设备ID管理

#### relay-tunnel工具
- 轻量级TCP隧道工具
- 支持多会话SSH
- 三层降级架构（UDP打洞 → TCP打洞 → Relay）

### 安全特性

- TLS 1.3支持
- JWT认证
- Token Bucket限速
- 自动封禁机制

### 部署支持

- Systemd服务配置
- Nginx TLS终结
- Let's Encrypt证书支持
- 完整的生产部署文档

## v0.9.0 (2023-12-01)

### 新增功能

- DCUtR协议实现（libp2p兼容）
- Circuit Relay v2协议
- 自动协议协商
- 多通道支持

### 改进

- 优化NAT穿透成功率
- 改进打洞算法
- 降低延迟

## v0.8.0 (2023-10-15)

### 新增功能

- Admin HTTP接口
- Prometheus指标导出
- 健康检查端点
- 配置热重载

### 改进

- 性能优化（提升30%）
- 内存占用降低
- 更好的错误处理

## v0.7.0 (2023-08-30)

### 新增功能

- Redis持久化支持
- 服务发现协议
- 动态端口分配

### 改进

- 连接稳定性提升
- 重连机制优化

## v0.6.0 (2023-07-01)

### 新增功能

- WSS（WebSocket Secure）支持
- HTTP CONNECT代理支持
- DPI对抗能力

### 改进

- 企业防火墙穿透能力提升
- 协议伪装增强

## v0.5.0 (2023-05-15)

### 新增功能

- Simple Relay Protocol
- relay-tunnel工具
- SSH隧道支持

### 改进

- TCP打洞实现
- 连接建立速度提升

## v0.4.0 (2023-03-30)

### 新增功能

- STUN服务器
- NAT类型检测
- UDP打洞实现

### 改进

- P2P连接成功率提升至80%（Cone NAT）

## v0.3.0 (2023-02-15)

### 新增功能

- Signaling Server
- 设备注册
- 信令交换
- 基础P2P连接

### 改进

- 初始C++ SDK实现
- 基础协议实现

## 版本说明

### 语义化版本

PeerLink使用语义化版本（Semver）：`MAJOR.MINOR.PATCH`

- **MAJOR**: 不兼容的API变更
- **MINOR**: 向后兼容的新功能
- **PATCH**: 向后兼容的问题修复

### 支持策略

- **当前版本**: v1.0.x（完全支持）
- **上一个版本**: v0.9.x（维护支持）
- **更早版本**: 不再支持

### 升级建议

**从v0.9.x升级到v1.0.0:**
```bash
# 备份配置
sudo cp /etc/peerlink/peerlink.toml /etc/peerlink/peerlink.toml.bak

# 升级
sudo apt-get update && sudo apt-get install peerlink

# 检查配置变化
# 注意：JWT_SECRET现在是必需的

# 重启服务
sudo systemctl restart peerlink-gateway peerlink-network
```

### 即将发布

#### v1.1.0（计划中）

- IPv6完整支持
- QUIC协议支持
- 更多语言绑定（Java, Go）
- WebRTC支持

#### v1.2.0（计划中）

- 分布式Relay网络
- 自动负载均衡
- 区域化部署支持
- 更多监控指标
