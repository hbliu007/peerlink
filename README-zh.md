# PeerLink

<div align="center">

![PeerLink Logo](.github/logo.svg)

### 高性能 P2P 通信库

*🌐 WebRTC 原生支持 • 🔒 TLS 1.3 + Noise • ⚡ 500 Mbps • 🔄 libp2p 兼容*

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++20](https://img.shields.io/badge/C++-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/CMake-3.20+-green.svg)](https://cmake.org/)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20Windows-white.svg)](https://github.com/hbliu007/peerlink)
[![Stars](https://img.shields.io/github/stars/hbliu007/peerlink?style=social)](https://github.com/hbliu007/peerlink)
[![Contributors](https://img.shields.io/github/contributors/hbliu007/peerlink)](https://github.com/hbliu007/peerlink/graphs/contributors)

[快速开始](#-快速开始) • [功能特性](#-功能特性) • [架构](#-架构) • [文档](#-文档) • [贡献](#-贡献)

[English](./README.md) • 中文

</div>

---

![PeerLink P2P 网络](.github/images/peerlink-hero.png)

---

## 🚀 快速开始

### 前置要求

- CMake 3.20+
- C++20 编译器 (GCC 11+, Clang 14+, MSVC 2022+)
- OpenSSL 3.0+

### 构建

```bash
git clone https://github.com/hbliu007/peerlink.git
cd peerlink/p2p-cpp

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

cd build && ctest -V
```

### Docker

```bash
docker run -it hbliu007/peerlink:latest
```

---

## ✨ 功能特性

### 🔐 安全协议
- TLS 1.3 (`/tls/1.0.0`)
- Noise 协议 (`/noise`)

### 🔀 流复用
- mplex (`/mplex/6.7.0`)
- yamux (`/yamux/1.0.0`)

### 🌐 传输层
| 协议 | 描述 |
|------|------|
| TCP | IPv4/IPv6 |
| QUIC | UDP 多路复用 |
| WebRTC | 浏览器到浏览器 |
| WebTransport | HTTP/3 |

### 🧭 节点路由
- Kademlia DHT — 分布式哈希表
- Bootstrap Nodes — 初始节点发现

### 📢 发布订阅
- GossipSub v1.1 — 可扩展发布订阅

### 🔓 NAT 穿透
| 方法 | 描述 |
|------|------|
| STUN | NAT 类型检测 |
| TURN | 中继回退 |
| ICE | 候选地址收集 |
| Hole Punching | 打洞直连 |

---

## 📊 性能指标

| 指标 | 数值 |
|------|------|
| P2P 吞吐量 | **500 Mbps** |
| 连接延迟 | **20ms** |
| 并发连接数 | **10,000+** |
| 内存占用 | **~50 MB** |

### 各协议吞吐量

| 协议 | 吞吐量 | 延迟 |
|------|--------|------|
| TCP 直连 | ~500 Mbps | ~20ms |
| QUIC | ~450 Mbps | ~15ms |
| WebRTC | ~400 Mbps | ~25ms |
| 中继 (TURN) | ~50 Mbps | ~100ms |

---

## 📂 项目结构

```
peerlink/
├── p2p-cpp/                  # C++ 核心库
│   ├── include/              # 公共头文件
│   │   └── p2p/             # 核心 API
│   │       ├── core/         # 引擎、连接、会话
│   │       ├── crypto/       # TLS、Noise、Ed25519
│   │       ├── multiaddr/    # 多地址实现
│   │       ├── net/          # 异步 I/O (Asio)
│   │       ├── protocol/     # libp2p 协议
│   │       └── transport/   # TCP、QUIC、WebRTC
│   ├── src/                  # 实现
│   │   ├── servers/          # STUN、TURN、信令、DID
│   │   └── tests/            # 单元测试和集成测试
│   └── examples/             # 使用示例
├── signaling-server-cpp/      # WebSocket 信令服务器
└── docs/                     # 架构和 API 文档
```

---

## 💻 API 使用

```cpp
#include <p2p/engine.hpp>

int main() {
    p2p::Config config;
    config.listen_addresses = {"/ip4/0.0.0.0/tcp/0"};
    config.enable_webrtc = true;

    auto engine = p2p::Engine::Create(config);
    engine->Start();

    auto conn = engine->Connect(p2p::PeerId::FromString("QmPeer..."));
    conn->Send("Hello, P2P!");

    engine->Stop();
    return 0;
}
```

---

## 📖 文档

- [架构设计](./p2p-cpp/docs/ARCHITECTURE.md)
- [API 参考](./p2p-cpp/docs/API.md)
- [示例代码](./p2p-cpp/examples/)

---

## 🤝 贡献

欢迎贡献！

```bash
git clone https://github.com/hbliu007/peerlink.git
git checkout -b feature/amazing-feature
cmake -B build -DBUILD_TESTS=ON
cmake --build build
git commit -m "feat: add amazing feature"
git push origin feature/amazing-feature
```

---

## 📄 许可证

PeerLink 使用 [MIT 许可证](./LICENSE)。

---

<div align="center">

**⭐ 如果觉得有用，请给个 Star！⭐**

[![GitHub Star](https://img.shields.io/github/stars/hbliu007/peerlink?style=social)](https://github.com/hbliu007/peerlink)
[![GitHub issues](https://img.shields.io/github/issues/hbliu007/peerlink?logo=github)](https://github.com/hbliu007/peerlink/issues)
[![GitHub PRs](https://img.shields.io/github/issues-pr/hbliu007/peerlink?logo=github)](https://github.com/hbliu007/peerlink/pulls)

由 ❤️ 打造 by [hbliu007](https://github.com/hbliu007)

</div>
