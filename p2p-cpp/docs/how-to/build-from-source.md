---
title: 从源码编译指南
description: 在Linux、macOS上编译PeerLink，包括依赖安装、cmake配置和交叉编译
tags:
  - build
  - cmake
  - cross-compile
  - dependencies
---

# 从源码编译指南

本指南介绍如何在Linux和macOS上从源码编译PeerLink。

## 系统要求

| 平台 | 最低要求 | 推荐 |
|------|----------|------|
| Linux | Ubuntu 20.04 | Ubuntu 22.04 LTS |
| macOS | macOS 11 | macOS 13+ |
| 编译器 | GCC 9 / Clang 12 | GCC 11 / Clang 14 |
| CMake | 3.18 | 3.25+ |
| 内存 | 2GB | 4GB+ |
| 磁盘 | 500MB | 1GB+ |

## Linux依赖安装

### Ubuntu/Debian

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    libboost-all-dev \
    libssl-dev \
    protobuf-compiler \
    libprotobuf-dev \
    nlohmann-json3-dev \
    libredis-plus-plus-dev
```

### CentOS/RHEL

```bash
sudo yum groupinstall -y "Development Tools"
sudo yum install -y \
    cmake \
    git \
    boost-devel \
    openssl-devel \
    protobuf-devel \
    protobuf-compiler \
    json-devel \
    redis-plus-plus-devel
```

### Fedora

```bash
sudo dnf install -y \
    gcc-c++ \
    cmake \
    git \
    boost-devel \
    openssl-devel \
    protobuf-devel \
    protobuf-compiler \
    json-devel \
    redis-plus-plus-devel
```

## macOS依赖安装

### 使用Homebrew

```bash
# 安装Homebrew（如果未安装）
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# 安装依赖
brew install cmake boost openssl protobuf nlohmann-json redis-plus-plus
```

### 设置OpenSSL路径

```bash
# 添加到~/.zshrc或~/.bash_profile
export OPENSSL_ROOT_DIR=$(brew --prefix openssl)
export PKG_CONFIG_PATH=$(brew --prefix openssl)/lib/pkgconfig
```

## 获取源码

```bash
# 克隆仓库
git clone https://github.com/your-org/p2p-cpp.git
cd p2p-cpp

# 或使用已有源码目录
cd /path/to/p2p-cpp
```

## CMake配置

### 基本配置

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

### 配置选项

| 选项 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `CMAKE_BUILD_TYPE` | string | `Release` | 构建类型 |
| `BUILD_SERVERS` | bool | `ON` | 构建服务器程序 |
| `BUILD_TESTS` | bool | `ON` | 构建测试程序 |
| `BUILD_BINDINGS` | bool | `ON` | 构建语言绑定 |
| `BUILD_PYTHON_BINDINGS` | bool | `ON` | 构建Python绑定 |
| `BUILD_C_BINDINGS` | bool | `ON` | 构建C API |
| `ENABLE sanitizers` | bool | `OFF` | 启用地址/未定义行为检测 |
| `ENABLE_COVERAGE` | bool | `OFF` | 启用代码覆盖率 |

### 调试构建

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug \
    -DENABLE_SANITIZERS=ON
```

### 最小构建

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=OFF \
    -DBUILD_BINDINGS=OFF
```

## 编译

### 编译所有目标

```bash
cmake --build build -j"$(nproc)"
```

### 编译特定目标

```bash
# 仅编译服务器
cmake --build build --target peerlink-gateway peerlink-network -j"$(nproc)"

# 仅编译客户端
cmake --build build --target peerlink-client -j"$(nproc)"

# 仅编译工具
cmake --build build --target relay-tunnel -j"$(nproc)"
```

### 编译输出

编译产物位于 `build/` 目录：

```
build/
├── src/
│   ├── servers/gateway/peerlink-gateway
│   ├── servers/network/peerlink-network
│   └── client/peerlink-client
├── tools/
│   └── p2p-tunnel/relay-tunnel
└── bindings/
    ├── python/
    └── c/
```

## 交叉编译

### ARM64 (aarch64)

#### 安装交叉编译工具链

```bash
# Ubuntu/Debian
sudo apt-get install -y \
    g++-aarch64-linux-gnu \
    gcc-aarch64-linux-gnu \
    qemu-user-static
```

#### 配置CMake

```bash
cmake -B build-aarch64 -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
    -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
    -CMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ \
    -DBUILD_TESTS=OFF
```

#### 编译

```bash
cmake --build build-aarch64 -j"$(nproc)"
```

### ARMv7 (armhf)

#### 安装工具链

```bash
sudo apt-get install -y \
    g++-arm-linux-gnueabihf \
    gcc-arm-linux-gnueabihf
```

#### 配置CMake

```bash
cmake -B build-armhf -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR=arm \
    -DCMAKE_C_COMPILER=arm-linux-gnueabihf-gcc \
    -DCMAKE_CXX_COMPILER=arm-linux-gnueabihf-g++ \
    -DBUILD_TESTS=OFF
```

## 测试

### 运行单元测试

```bash
# 编译测试
cmake --build build --target tests

# 运行所有测试
cd build
ctest --output-on-failure

# 运行特定测试
./tests/unit/test_rate_limiter
./tests/unit/test_message
```

### 运行集成测试

```bash
# 启动必要的服务
redis-server --daemonize yes

# 运行集成测试
./tests/integration/test_e2e.sh
```

### 代码覆盖率

```bash
# 配置覆盖率构建
cmake -B build-coverage -DCMAKE_BUILD_TYPE=Debug \
    -DENABLE_COVERAGE=ON

# 编译并运行测试
cmake --build build-coverage --target tests
cd build-coverage
ctest

# 生成覆盖率报告
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
```

## 安装

### 系统范围安装

```bash
sudo cmake --install build --strip \
    --prefix /usr/local \
    --component Runtime

# 或安装到opt目录
sudo cmake --install build --strip \
    --prefix /opt/peerlink
```

### 打包

```bash
# 创建DEB包
cd build
cpack -G DEB

# 创建RPM包
cpack -G RPM

# 创建TGZ包
cpack -G TGZ
```

## 高级选项

### 静态链接

```bash
cmake -B build-static -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_EXE_LINKER_FLAGS="-static-libstdc++"
```

### 优化构建

```bash
cmake -B build-optimized -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-O3 -march=native"
```

### Verbose输出

```bash
cmake --build build --verbose
```

## 故障排查

### 编译错误

```bash
# 清理构建目录
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

### 依赖问题

```bash
# 检查Boost版本
dpkg -s libboost-all-dev | grep Version

# 检查OpenSSL版本
openssl version

# 检查Protobuf版本
protoc --version
```

### CMake缓存问题

```bash
# 删除CMake缓存
rm build/CMakeCache.txt
cmake -B build
```

## Docker构建

### Dockerfile示例

```dockerfile
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential cmake git \
    libboost-all-dev libssl-dev \
    protobuf-compiler libprotobuf-dev \
    nlohmann-json3-dev

WORKDIR /build
COPY . .

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build -- -j$(nproc)
```

### 构建镜像

```bash
docker build -t peerlink:latest .
```

## 常见编译目标

| 目标 | 说明 |
|------|------|
| `peerlink-gateway` | 网关服务器 |
| `peerlink-network` | 网络服务（STUN/TURN） |
| `peerlink-client` | 客户端库 |
| `simple-relay-server` | 简单中继服务器 |
| `relay-tunnel` | 隧道工具 |
| `tests` | 所有测试 |
| `install` | 安装目标 |
