<p align="center">
  <img src="https://img.shields.io/badge/BTO-Back_to_Office-3b82f6?style=for-the-badge&labelColor=0f172a" alt="BTO">
</p>

<h1 align="center">Back To Office</h1>

<p align="center">
  <strong>一行命令，从家里 SSH 到办公室电脑</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Powered_By-PeerLink-10b981?style=flat-square" alt="Powered by PeerLink">
  <img src="https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20Android-blue?style=flat-square" alt="Platform">
  <img src="https://img.shields.io/badge/License-MIT-yellow?style=flat-square" alt="License: MIT">
  <img src="https://img.shields.io/badge/C%2B%2B-20-blue?style=flat-square&logo=c%2B%2B" alt="C++20">
</p>

<p align="center">
  <a href="#quick-start">Quick Start</a> · <a href="#usage">Usage</a> · <a href="#how-it-works">How It Works</a> · <a href="#build">Build</a>
</p>

---

## What is BTO?

**Back To Office** (bto) 是基于 [PeerLink](https://github.com/hbliu007/peerlink) P2P 平台的办公远程访问工具。

核心场景：**在家里用一行命令 SSH 到公司防火墙后面的办公电脑**。

```console
$ bto connect office-213
✓ Connected to office-213 via UDP direct (12ms)
✓ SSH tunnel ready on localhost:9022

$ ssh -p 9022 user@localhost
user@office-213:~$
```

> [!IMPORTANT]
> 无需公网 IP · 无需开放端口 · 无需 VPN · 无需第三方账号

## Quick Start

### Prerequisites

- 一台公网服务器（部署 PeerLink Relay）
- 办公电脑（Linux，在公司防火墙后面）
- 你的笔记本（macOS / Linux）或手机（Android Termux）

### 1. Deploy Relay Server

```bash
# 在公网服务器上
git clone https://github.com/hbliu007/peerlink.git
cd peerlink/p2p-cpp && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)
./build/src/servers/signaling/p2p-signaling-server
```

### 2. Office Machine

```bash
# 在办公电脑上
bto serve --did office-213 --relay your-server.com:9443 --forward 127.0.0.1:22
✓ Registered as office-213
✓ Forwarding to 127.0.0.1:22
```

### 3. Your Laptop

```bash
# 在家里
bto connect office-213 --relay your-server.com:9443 --listen 9022
✓ Connected via UDP direct (12ms)
✓ Listening on localhost:9022

ssh -p 9022 user@localhost
user@office-213:~$
```

## Usage

### Configuration File

创建 `~/.bto/config.toml`：

```toml
[defaults]
relay = "your-server.com:9443"
identity = "home-laptop"

[hosts.office-213]
user = "dev"
port = 22

[hosts.office-215]
user = "dev"
port = 22

[hosts.office-gpu]
user = "researcher"
port = 22
```

配置后一行命令即可连接：

```bash
bto connect office-213     # 自动使用配置文件的 relay 和 user
bto connect office-gpu     # 切换到另一台办公机
```

### Multi-Session

```bash
# 同时打开多个 SSH 会话，互不影响
bto connect office-213 --listen 9022

# 终端 1
ssh -p 9022 user@localhost

# 终端 2（同时）
ssh -p 9022 user@localhost
```

### Mobile (Android Termux)

```bash
# 手机上安装
pkg install cmake clang boost openssl
bto connect office-213 --relay your-server.com:9443 --listen 9022

# 在手机 Termux 中 SSH
ssh -p 9022 user@localhost
```

## How It Works

```
┌──────────────┐                          ┌──────────────┐
│   Your Mac   │                          │ Office PC    │
│   (Home)     │                          │ (Corporate)  │
└──────┬───────┘                          └──────┬───────┘
       │                                         │
       │  bto connect office-213                 │  bto serve
       │                                         │
       ├──── UDP Direct (80%) ───────────────────┤
       │     12ms latency, 500+ Mbps             │
       │                                         │
       ├──── TCP Punch (+15%) ───────────────────┤
       │     Fallback for strict NAT             │
       │                                         │
       └──── TLS Relay (100%) ───────────────────┘
             via PeerLink Relay Server
```

### Architecture

| Component | Role |
|:----------|------|
| **bto CLI** | Command-line interface for tunnel management |
| **relay-tunnel** | P2P tunnel engine (multi-session TCP bridge) |
| **PeerLink SDK** | NAT traversal, DID identity, TLS 1.3 |
| **Relay Server** | Signaling + relay fallback on public VPS |

### Features

| Feature | Status | Description |
|:--------|:------:|:------------|
| SSH Tunnel | ✅ | Forward SSH through P2P connection |
| Multi-Session | ✅ | Multiple concurrent SSH sessions |
| Config File | ✅ | `~/.bto/config.toml` with host aliases |
| NAT Traversal | ✅ | 3-layer: UDP → TCP → TLS Relay |
| Auto Reconnect | 🔄 | Automatic reconnection on disconnect |
| SOCKS Proxy | 🔄 | Generic TCP proxy mode |
| Port Forwarding | ✅ | Forward any TCP port, not just SSH |

## Build

```bash
# Prerequisites: CMake 3.20+, C++20 compiler, OpenSSL 3.0+, Boost

git clone https://github.com/hbliu007/back-to-office.git
cd back-to-office

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Binary at: build/bto
```

## Related Projects

| Project | Description |
|:--------|:------------|
| [PeerLink](https://github.com/hbliu007/peerlink) | P2P communication platform (core SDK) |
| [rPPG-SDK](https://github.com/hbliu007/peerlink/tree/main/rPPG) | Remote PPG heart rate monitoring SDK |

## Contributing

Contributions welcome! Please read the [Contributing Guide](CONTRIBUTING.md).

## License

[MIT License](LICENSE) — Free for commercial use.

---

<p align="center">
  <strong>Back To Office</strong> — 因为有时候你只需要 SSH 进去
</p>
