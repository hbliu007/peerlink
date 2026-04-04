# 测试指南

PeerLink 的测试策略和最佳实践。

---

## 测试策略

PeerLink 采用测试金字塔策略：

```mermaid
graph TB
    subgraph "测试金字塔"
        E2E[端到端测试<br/>10%]
        INT[集成测试<br/>30%]
        UNIT[单元测试<br/>60%]
    end

    E2E --> INT
    INT --> UNIT
```

- **单元测试 (60%)**: 测试单个函数/类
- **集成测试 (30%)**: 测试模块间交互
- **端到端测试 (10%)**: 测试完整场景

---

## 测试框架

### Google Test

用于 C++ 单元测试和集成测试。

### Catch2

替代方案，用于 BDD 风格测试。

### 测试工具

- **Google Mock**: 模拟对象
- **Fakeit**: 轻量级模拟框架
- **HttpServer**: 测试 HTTP/WebSocket

---

## 单元测试

### 目录结构

```
tests/
├── unit/
│   ├── core/
│   │   ├── event_loop_test.cpp
│   │   └── memory_pool_test.cpp
│   ├── protocol/
│   │   ├── signaling_test.cpp
│   │   └── p2p_protocol_test.cpp
│   ├── transport/
│   │   ├── udp_socket_test.cpp
│   │   └── tcp_socket_test.cpp
│   ├── nat/
│   │   ├── stun_test.cpp
│   │   └── hole_punching_test.cpp
│   └── security/
│       ├── did_test.cpp
│       └── tls_test.cpp
```

### 编写单元测试

```cpp
#include <gtest/gtest.h>
#include <peerlink/memory_pool.hpp>

namespace peerlink {
namespace test {

class MemoryPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        pool_ = std::make_unique<MemoryPool>();
    }

    void TearDown() override {
        pool_.reset();
    }

    std::unique_ptr<MemoryPool> pool_;
};

TEST_F(MemoryPoolTest, AllocateSmallObject) {
    // Arrange
    const size_t size = 1024;

    // Act
    void* ptr = pool_->Allocate(size);

    // Assert
    ASSERT_NE(ptr, nullptr);
    pool_->Deallocate(ptr);
}

TEST_F(MemoryPoolTest, AllocateLargeObject) {
    const size_t size = 1024 * 1024;  // 1MB
    void* ptr = pool_->Allocate(size);
    ASSERT_NE(ptr, nullptr);
    pool_->Deallocate(ptr);
}

TEST_F(MemoryPoolTest, Statistics) {
    pool_->Allocate(1024);
    pool_->Allocate(2048);

    auto stats = pool_->GetStats();
    EXPECT_EQ(stats.allocation_count, 2);
    EXPECT_EQ(stats.total_bytes, 3072);
}

}  // namespace test
}  // namespace peerlink
```

### 使用 Mock

```cpp
#include <gmock/gmock.h>
#include <peerlink/transport.hpp>

class MockTransport : public Transport {
public:
    MOCK_METHOD(Future<void>, Connect, (const Endpoint&), (override));
    MOCK_METHOD(Future<void>, Send, (const Bytes&), (override));
    MOCK_METHOD(void, Close, (), (override));
};

TEST(P2PClientTest, UsesTransportToConnect) {
    auto mock_transport = std::make_shared<MockTransport>();

    EXPECT_CALL(*mock_transport, Connect(_))
        .Times(1)
        .WillOnce(Return(ByMove(make_ready_future())));

    P2PClient client(mock_transport);
    client.Connect("peer_id").get();
}
```

---

## 集成测试

### 测试场景

1. **Signaling 流程**: 完整的会话建立
2. **NAT 穿透**: UDP/TCP 打洞
3. **数据传输**: 可靠传输协议
4. **中继降级**: 直连失败后的中继

### 示例：Signaling 集成测试

```cpp
#include <gtest/gtest.h>
#include <peerlink/signaling_client.hpp>
#include <peerlink/test/signaling_server.hpp>

class SignalingIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        server_ = std::make_unique<test::SignalingServer>();
        server_->Start();

        client_ = std::make_unique<SignalingClient>();
        client_->Connect(server_->GetEndpoint());
    }

    void TearDown() override {
        client_->Disconnect();
        server_->Stop();
    }

    std::unique_ptr<test::SignalingServer> server_;
    std::unique_ptr<SignalingClient> client_;
};

TEST_F(SignalingIntegrationTest, ConnectAndDisconnect) {
    ASSERT_TRUE(client_->IsConnected());

    client_->Disconnect();
    ASSERT_FALSE(client_->IsConnected());
}

TEST_F(SignalingIntegrationTest, ExchangeOffers) {
    std::string peer_id = "did:peer:example";
    auto offer = client_->CreateOffer(peer_id);

    ASSERT_FALSE(offer.IsEmpty());
    ASSERT_EQ(offer.GetPeerId(), peer_id);
}
```

---

## 端到端测试

### 测试场景

1. **完整连接流程**: 两个客户端建立 P2P 连接
2. **数据传输**: 大文件传输
3. **NAT 穿透**: 各种 NAT 类型
4. **中继模式**: 直连失败后的中继

### 示例：E2E 测试

```cpp
#include <gtest/gtest.h>
#include <peerlink/p2p_client.hpp>
#include <peerlink/test/test_environment.hpp>

class E2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        env_ = std::make_unique<test::TestEnvironment>();
        env_->Start();

        client_a_ = std::make_unique<P2PClient>(env_->GetConfig());
        client_b_ = std::make_unique<P2PClient>(env_->GetConfig());

        client_a_->Start();
        client_b_->Start();
    }

    void TearDown() override {
        client_a_->Stop();
        client_b_->Stop();
        env_->Stop();
    }

    std::unique_ptr<test::TestEnvironment> env_;
    std::unique_ptr<P2PClient> client_a_;
    std::unique_ptr<P2PClient> client_b_;
};

TEST_F(E2ETest, EstablishConnection) {
    auto session_future = client_a_->Connect(client_b_->GetPeerId());
    auto session = session_future.get();

    ASSERT_TRUE(session.IsConnected());
    ASSERT_EQ(session.GetRemotePeerId(), client_b_->GetPeerId());
}

TEST_F(E2ETest, SendAndReceive) {
    auto session = client_a_->Connect(client_b_->GetPeerId()).get();

    std::string test_data = "Hello, PeerLink!";
    session.Send(Bytes(test_data)).get();

    // 等待接收
    auto received = client_b_->WaitForData(std::chrono::seconds(5));
    ASSERT_EQ(received.ToString(), test_data);
}
```

---

## 测试覆盖

### 目标覆盖率

- **总体**: > 80%
- **核心模块**: > 90%
- **关键路径**: 100%

### 生成覆盖率报告

```bash
# 构建带覆盖率的版本
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
make

# 运行测试
make test

# 生成报告
make coverage

# 查看 HTML 报告
open coverage/index.html
```

### 覆盖率工具

- **gcov/lcov**: Linux
- **Xcode Code Coverage**: macOS
- **OpenCPPCoverage**: Windows

---

## 性能测试

### 基准测试

使用 Google Benchmark：

```cpp
#include <benchmark/benchmark.h>
#include <peerlink/memory_pool.hpp>

static void BM_MemoryPoolAllocate(benchmark::State& state) {
    MemoryPool pool;
    for (auto _ : state) {
        void* ptr = pool.Allocate(1024);
        benchmark::DoNotOptimize(ptr);
        pool.Deallocate(ptr);
    }
}
BENCHMARK(BM_MemoryPoolAllocate);

BENCHMARK_MAIN();
```

### 运行基准测试

```bash
./benchmarks/memory_pool_benchmark
```

### 性能回归检测

```bash
# 运行基准测试并比较
./benchmarks/memory_pool_benchmark --benchmark_repetitions=10
```

---

## 压力测试

### 场景

1. **大量并发连接**: 1000+ 同时连接
2. **大数据传输**: GB 级文件传输
3. **长时间运行**: 24小时稳定性测试

### 示例：压力测试

```cpp
TEST(StressTest, ThousandConnections) {
    P2PClient client;
    client.Start();

    std::vector<Session> sessions;
    for (int i = 0; i < 1000; i++) {
        auto session = client.Connect(GetRandomPeerId()).get();
        sessions.push_back(session);
    }

    // 验证所有连接都活跃
    for (auto& session : sessions) {
        ASSERT_TRUE(session.IsConnected());
    }
}

TEST(StressTest, LargeFileTransfer) {
    auto session = EstablishConnection();

    // 生成 1GB 数据
    Bytes data(1024 * 1024 * 1024);
    FillRandom(data);

    auto start = std::chrono::steady_clock::now();
    session.Send(data).get();
    auto end = std::chrono::steady_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);
    ASSERT_LT(duration.count(), 60);  // 应该在 60 秒内完成
}
```

---

## 模糊测试

### 使用 AFL

```bash
# 安装 AFL
sudo apt-get install afl++

# 构建可执行文件
afl-g++ -o fuzz_target fuzz_target.cpp

# 运行模糊测试
afl-fuzz -i input_cases -o findings ./fuzz_target
```

### 使用 libFuzzer

```cpp
#include <stddef.h>
#include <stdint.h>
#include <peerlink/protocol_parser.hpp>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    peerlink::ProtocolParser parser;
    parser.Parse(Bytes(data, size));
    return 0;
}
```

---

## 测试最佳实践

### 1. 测试独立性

每个测试应该独立运行，不依赖其他测试：

```cpp
// 不好：依赖测试顺序
TEST_F(MyTest, Step1) { shared_state = 1; }
TEST_F(MyTest, Step2) { ASSERT_EQ(shared_state, 1); }

// 好：每个测试独立
TEST_F(MyTest, Step1) {
    shared_state = 1;
    ASSERT_EQ(shared_state, 1);
}
TEST_F(MyTest, Step2) {
    shared_state = 1;
    ASSERT_EQ(shared_state, 1);
}
```

### 2. 使用测试夹具

复用测试设置：

```cpp
class MyTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        // 设置
    }
    void TearDown() override {
        // 清理
    }

    // 共享资源
    std::unique_ptr<MyClass> object_;
};
```

### 3. 明确的测试名称

```cpp
// 不好
TEST(Test, Test1) { }

// 好
TEST(P2PClient, ConnectSuccess_SymmetricNat) { }
```

### 4. 有意义的断言消息

```cpp
ASSERT_EQ(session.GetState(), Session::State::kConnected)
    << "Session state should be CONNECTED, but got: "
    << static_cast<int>(session.GetState());
```

---

## CI/CD 集成

### GitHub Actions 示例

```yaml
name: Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Install dependencies
        run: sudo apt-get install -y cmake g++ libssl-dev
      - name: Build
        run: |
          mkdir build && cd build
          cmake .. -DBUILD_TESTS=ON
          make -j$(nproc)
      - name: Run tests
        run: |
          cd build
          ctest --output-on-failure
      - name: Upload coverage
        run: |
          cd build
          make coverage
          bash <(curl -s https://codecov.io/bash)
```

---

**下一步**: [开发指南](development.md) · [贡献指南](overview.md)
