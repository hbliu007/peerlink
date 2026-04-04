# macOS 构建说明

## 问题描述

在 macOS 上使用 Conda/Miniforge 环境时，可能遇到编译错误：

```
error: expected ')'
see https://conda-forge.org/docs/maintainer/knowledge_base.html#newer-c-features-with-old-sdk
```

**根本原因**：
- Homebrew 的 Protobuf 依赖 Abseil，使用了 C++20 的 `from_chars` 浮点支持
- Conda/Miniforge 提供的旧版 macOS SDK 不支持此特性
- 编译器优先使用 Conda 的 libc++ 头文件，导致编译失败

## 解决方案

使用提供的构建脚本，它会自动清理 PATH 环境变量，强制使用系统的 Xcode 工具链：

### 增量构建
```bash
./build.sh [Release|Debug] [target]
```

示例：
```bash
./build.sh                    # 默认 Release 构建所有目标
./build.sh Debug              # Debug 构建所有目标
./build.sh Release basic_client  # 只构建 basic_client
```

### 清理重建
```bash
./clean_build.sh [Release|Debug]
```

示例：
```bash
./clean_build.sh              # 清理并 Release 构建
./clean_build.sh Debug        # 清理并 Debug 构建
```

## 手动构建

如果需要手动构建，请使用干净的 PATH：

```bash
# 清理 PATH
export PATH=/usr/bin:/bin:/usr/sbin:/sbin:/Library/Developer/CommandLineTools/usr/bin:/opt/homebrew/bin

# 配置
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=ON -DBUILD_SHARED_LIBS=OFF

# 构建
cmake --build build -j$(sysctl -n hw.ncpu)
```

## 验证

构建成功后，可执行文件位于：
- `build/examples/basic/basic_client` - 基础客户端示例
- `build/examples/basic/basic_server` - 基础服务器示例

## 依赖要求

- macOS 10.15+
- Xcode Command Line Tools
- Homebrew 包：
  - boost (>= 1.70)
  - openssl@3
  - protobuf (>= 3.0)
  - gtest (用于测试)
  - nlohmann-json

安装依赖：
```bash
brew install boost openssl@3 protobuf gtest nlohmann-json
```
