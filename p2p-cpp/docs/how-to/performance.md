# 性能优化

优化 PeerLink 性能的建议和最佳实践。

---

## 基准性能

在千兆网络环境下，PeerLink 的目标性能指标：

| 指标 | 直连 | 中继 |
|------|------|------|
| **吞吐量** | > 500 Mbps | > 50 Mbps |
| **延迟** | < 10 ms | < 50 ms |
| **连接建立** | < 2 s | < 5 s |
| **内存占用** | < 50 MB | < 100 MB |
| **CPU 占用** | < 10% | < 20% |

---

## 网络优化

### 1. 优先使用 UDP

UDP 通常比 TCP 有更低的延迟和更高的吞吐量：

```yaml
# ~/.peerlink/config.yaml
network:
  transport:
    preferred: udp
```

### 2. 调整缓冲区大小

根据网络条件调整缓冲区：

```yaml
performance:
  # 高吞吐量场景
  buffer_size: 262144  # 256KB

  # 低延迟场景
  buffer_size: 16384   # 16KB
```

### 3. 启用 TCP_NODELAY

禁用 Nagle 算法，减少延迟：

```yaml
performance:
  tcp:
    no_delay: true
```

---

## 内存优化

### 1. 限制内存使用

```yaml
performance:
  memory_limit: 512  # MB
```

### 2. 调整连接池大小

```yaml
performance:
  connection_pool: 100  # 根据并发连接数调整
```

### 3. 使用内存池

PeerLink 内部使用内存池减少分配开销：

```yaml
performance:
  memory_pool:
    enabled: true
    small_object_size: 4096  # <= 4KB 使用小对象分配器
    large_object_threshold: 1048576  # >= 1MB 使用大对象分配器
```

---

## CPU 优化

### 1. 限制 CPU 使用

```yaml
performance:
  cpu_limit: 0.5  # 50% CPU
```

### 2. 调整线程池大小

```yaml
performance:
  thread_pool:
    size: 4  # 根据 CPU 核心数调整
    queue_size: 1000
```

### 3. 使用事件循环

PeerLink 使用事件循环模型，避免线程切换开销：

```yaml
performance:
  event_loop:
    type: epoll  # Linux
    # type: kqueue  # macOS/BSD
    # type: IOCP  # Windows
```

---

## NAT 穿透优化

### 1. 减少打洞时间

```yaml
network:
  nat:
    punch_timeout: 5  # 秒，默认 10
```

### 2. 并发尝试 UDP 和 TCP 打洞

```yaml
network:
  nat:
    concurrent_punching: true
```

### 3. 智能 Relay 选择

```yaml
network:
  nat:
    relay_selection: least-latency  # 选择延迟最低的 Relay
    # relay_selection: least-connections  # 选择连接数最少的 Relay
    # relay_selection: round-robin  # 轮询
```

---

## 应用层优化

### 1. 使用连接复用

避免频繁建立连接：

```cpp
// 错误：每次请求都新建连接
for (auto& request : requests) {
    auto session = client.connect(peer_id).get();
    session.send(request);
    session.close();
}

// 正确：复用连接
auto session = client.connect(peer_id).get();
for (auto& request : requests) {
    session.send(request);
}
session.close();
```

### 2. 批量发送数据

```cpp
// 错误：频繁发送小数据包
for (int i = 0; i < 1000; i++) {
    session.send(small_data);
}

// 正确：批量发送
std::vector<Bytes> batch;
for (int i = 0; i < 1000; i++) {
    batch.push_back(small_data);
}
session.send_batch(batch);
```

### 3. 使用零拷贝

```cpp
// 使用零拷贝 API
session.send_zero_copy(buffer, size);
```

---

## 监控和调优

### 1. 启用性能监控

```yaml
monitoring:
  metrics:
    enabled: true
    export_interval: 10s
    export_format: prometheus
```

### 2. 查看性能指标

```bash
# 查看连接统计
peerlink stats sess_abc123

# 查看系统资源使用
peerlink stats --resource

# 查看实时性能
peerlink monitor
```

### 3. 性能基准测试

```bash
# 测试吞吐量
peerlink benchmark sess_abc123 --mode throughput

# 测试延迟
peerlink benchmark sess_abc123 --mode latency

# 测试并发
peerlink benchmark sess_abc123 --mode concurrent --connections 10
```

---

## 特定场景优化

### 场景 1: 文件传输

**目标**：最大化吞吐量

```yaml
performance:
  buffer_size: 1048576  # 1MB
  tcp:
    window_size: 10485760  # 10MB
    no_delay: false  # 允许 Nagle 算法聚合小包
network:
  transport:
    preferred: udp  # UDP 更适合高吞吐
```

### 场景 2: 实时通信

**目标**：最小化延迟

```yaml
performance:
  buffer_size: 4096  # 4KB，减少缓冲
  low_latency: true
  tcp:
    no_delay: true  # 禁用 Nagle 算法
network:
  transport:
    preferred: udp
  nat:
    punch_timeout: 3  # 快速降级到中继
```

### 场景 3: 大规模部署

**目标**：最小化资源占用

```yaml
performance:
  memory_limit: 256  # MB
  cpu_limit: 0.3  # 30% CPU
  connection_pool: 50
  buffer_size: 16384  # 16KB
monitoring:
  metrics:
    enabled: true  # 监控资源使用
```

---

## 性能问题诊断

### 1. 识别瓶颈

```bash
# 查看性能指标
peerlink stats --detail

# 输出示例:
# Throughput: 123 Mbps
# Latency: p50=5ms, p95=15ms, p99=50ms
# CPU: 15%
# Memory: 75 MB
# Retransmissions: 0.5%
```

### 2. 瓶颈分析

| 症状 | 可能原因 | 解决方案 |
|------|---------|---------|
| 低吞吐量 | 缓冲区太小 | 增大 `buffer_size` |
| 高延迟 | 启用了 Nagle | 设置 `tcp.no_delay: true` |
| 高 CPU | 加密开销 | 考虑硬件加速 |
| 高内存 | 连接数过多 | 限制 `connection_pool` |
| 高重传率 | 网络拥塞 | 降低发送速率 |

### 3. 性能分析工具

```bash
# CPU 火焰图
peerlink flamegraph --output flamegraph.svg

# 内存分析
peerlink memory-profile --output memory.txt

# 网络分析
peerlink network-profile sess_abc123 --output network.txt
```

---

**下一步**: [配置指南](configuration.md) · [故障排查](troubleshooting.md)
