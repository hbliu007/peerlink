# 概念

深入理解 PeerLink 的工作原理。适合想了解"为什么这样设计"的读者。

## 核心概念

- **[PeerLink 如何工作](how-peerlink-works.md)** — 系统性长文，从问题到解决方案
- **[架构总览](architecture.md)** — 六层架构栈详解

## 网络协议

- **[NAT 穿透原理](nat-traversal.md)** — UDP/TCP 打洞机制
- **[DCUtR 协议](dcutr-protocol.md)** — libp2p 直连升级协议
- **[Circuit Relay](circuit-relay.md)** — 中继转发协议

## 安全与性能

- **[安全模型](security-model.md)** — Ed25519 + TLS 1.3 安全架构
- **[性能基准](performance.md)** — 延迟、吞吐量基准测试
- **[竞品对比](comparisons.md)** — vs Tailscale / FRP / ZeroTier
