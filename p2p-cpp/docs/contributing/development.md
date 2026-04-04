# 开发指南

PeerLink 开发的详细指南。

---

## 开发环境

### 前提条件

- **C++ 编译器**: GCC 11+ / Clang 13+ / MSVC 2022+
- **CMake**: 3.20+
- **Python**: 3.8+（用于测试脚本）
- **Docker**（可选，用于容器化开发）

### 推荐工具

- **IDE**: CLion / VS Code / Visual Studio
- **调试器**: GDB / LLDB
- **性能分析**: perf / Valgrind
- **内存检查**: Valgrind / AddressSanitizer

---

## 获取源码

### 克隆仓库

```bash
git clone https://github.com/your-org/peerlink.git
cd peerlink
```

### 子模块

```bash
# 初始化子模块
git submodule update --init --recursive
```

---

## 构建项目

### Debug 构建

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

### Release 构建

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### 自定义构建选项

```bash
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON \
  -DBUILD_BENCHMARKS=ON \
  -DENABLE_ASAN=ON \
  -DENABLE_UBSAN=ON
```

### 清理构建

```bash
make clean
# 或
rm -rf build
```

---

## 项目结构

```
peerlink/
├── include/           # 公共头文件
│   └── peerlink/     # 库的头文件
├── src/              # 源代码
│   ├── core/        # 核心模块
│   ├── protocol/    # 协议实现
│   ├── transport/   # 传输层
│   ├── nat/        # NAT 穿透
│   └── security/   # 安全模块
├── tests/           # 测试
│   ├── unit/       # 单元测试
│   ├── integration/ # 集成测试
│   └── e2e/        # 端到端测试
├── examples/        # 示例代码
├── tools/          # 开发工具
├── docs/           # 文档
├── CMakeLists.txt  # CMake 配置
└── README.md       # 项目说明
```

---

## 代码规范

### C++ 编码规范

遵循 [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)：

```cpp
// 类名：PascalCase
class P2PClient {
public:
    // 公共方法：PascalCase
    void ConnectToPeer();

private:
    // 私有方法：PascalCase
    void DoConnect();

    // 成员变量：trailing_underscore_
    int connection_count_;
};

// 函数名：PascalCase
void SendMessage();

// 变量名：snake_case
int message_count = 0;

// 常量：kPascalCase
constexpr int kMaxConnections = 100;

// 枚举：PascalCase
enum class ConnectionState {
    kConnecting,
    kConnected,
    kDisconnected
};
```

### 命名约定

| 类型 | 约定 | 示例 |
|------|------|------|
| 类 | PascalCase | `P2PClient` |
| 函数 | PascalCase | `SendMessage()` |
| 变量 | snake_case | `message_count` |
| 成员变量 | trailing_underscore_ | `connection_count_` |
| 常量 | kPascalCase | `kMaxConnections` |
| 枚举类 | PascalCase | `ConnectionState` |
| 枚举值 | kPascalCase | `kConnected` |

### 文件组织

- **头文件**: `.hpp` 或 `.h`
- **源文件**: `.cpp`
- **每个类一个文件**: `p2p_client.hpp` + `p2p_client.cpp`
- **最大行数**: 头文件 < 300 行，源文件 < 500 行

### 注释规范

```cpp
/**
 * @brief 连接到远程节点
 *
 * @param peer_id 远程节点的 Peer ID
 * @param options 连接选项
 * @return Future<Session> 连接成功的 Session
 *
 * @throws ConnectionException 连接失败时抛出
 *
 * 示例:
 * @code
 * auto session = client.Connect(peer_id).get();
 * @endcode
 */
Future<Session> Connect(const std::string& peer_id,
                       const ConnectOptions& options = {});
```

---

## 调试

### 使用 GDB

```bash
# 构建 Debug 版本
cmake .. -DCMAKE_BUILD_TYPE=Debug
make

# 运行 GDB
gdb ./peerlink-daemon
```

常用 GDB 命令：

```bash
(gdb) break peerlink::P2PClient::Connect
(gdb) run
(gdb) backtrace
(gdb) print variable_name
(gdb) continue
```

### 使用 LLDB（macOS）

```bash
lldb ./peerlink-daemon
(lldb) breakpoint set --name peerlink::P2PClient::Connect
(lldb) run
(lldb) bt
(lldb) frame variable
(lldb) continue
```

### 日志调试

```cpp
// 启用调试日志
peerlink::logging::SetLevel(peerlink::logging::Level::Debug);

// 添加日志
LOG(DEBUG) << "Connecting to peer: " << peer_id;
LOG(INFO) << "Connection established";
LOG(WARNING) << "Connection unstable";
LOG(ERROR) << "Connection failed";
```

---

## 性能分析

### 使用 perf（Linux）

```bash
# CPU 性能分析
perf record -p $(pidof peerlink-daemon)
perf report

# 火焰图
perf script | stackcollapse-perf.pl | flamegraph.pl > flamegraph.svg
```

### 使用 Valgrind

```bash
# 内存泄漏检测
valgrind --leak-check=full --show-leak-kinds=all ./peerlink-daemon

# 性能分析
valgrind --tool=callgrind ./peerlink-daemon
kcachegrind callgrind.out.<pid>
```

### 基准测试

```bash
# 运行基准测试
make benchmark

# 特定基准
./benchmarks/network_benchmark
```

---

## 测试

### 运行测试

```bash
# 所有测试
make test

# 特定测试
./tests/unit/p2p_client_test

# 带覆盖率
make coverage
```

### 编写测试

使用 Google Test 框架：

```cpp
#include <gtest/gtest.h>
#include <peerlink/p2p_client.hpp>

class P2PClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        client_ = std::make_unique<peerlink::P2PClient>(config_);
    }

    void TearDown() override {
        client_.reset();
    }

    peerlink::Config config_;
    std::unique_ptr<peerlink::P2PClient> client_;
};

TEST_F(P2PClientTest, ConnectSuccess) {
    auto future = client_->Connect("did:peer:example");
    // 测试逻辑...
}

TEST_F(P2PClientTest, ConnectFailure) {
    // 测试失败场景...
}
```

---

## 代码审查

### 提交前检查

```bash
# 格式检查
make check-format

# 静态分析
make analyze

# 运行测试
make test

# 检查覆盖率
make coverage
```

### 自查清单

- [ ] 代码符合格式规范
- [ ] 添加了必要的注释
- [ ] 添加了测试用例
- [ ] 测试通过
- [ ] 没有编译警告
- [ ] 没有内存泄漏
- [ ] 更新了文档

---

## 发布流程

### 版本号

遵循语义化版本 (Semantic Versioning)：

```
MAJOR.MINOR.PATCH

例: 1.2.3
- MAJOR: 不兼容的 API 变更
- MINOR: 向后兼容的功能添加
- PATCH: 向后兼容的 Bug 修复
```

### 发布步骤

1. 更新版本号
2. 更新 CHANGELOG
3. 创建 git tag
4. 构建发布包
5. 发布 GitHub Release

---

## 获取帮助

- **文档**: 查看 `docs/` 目录
- **示例**: 查看 `examples/` 目录
- **Issues**: 在 GitHub 提交问题
- **Discussions**: 在 GitHub Discussions 讨论

---

**下一步**: [测试指南](testing.md) · [贡献指南](overview.md)
