---
title: 限速配置指南
description: 配置Token Bucket限速防止DoS攻击，包括参数配置、场景推荐和监控指标
tags:
  - rate-limiting
  - security
  - token-bucket
  - dos-prevention
---

# 限速配置指南

本指南介绍如何配置PeerLink的Token Bucket限速机制，防止DoS攻击和资源滥用。

## Token Bucket算法

### 工作原理

每个客户端维护一个令牌桶：

```
请求到达 → 消耗1个令牌 → 允许/拒绝
                    ↓
              每秒补充令牌
```

- **Rate（速率）**：每秒补充的令牌数
- **Capacity（容量）**：桶的最大容量（突发大小）
- **Tokens（当前令牌）**：当前可用令牌数

### 封禁机制

- **Ban Threshold（阈值）**：违规次数达到此值触发封禁
- **Ban Duration（时长）**：封禁持续时间
- **Auto Unban**：封禁时长到期后自动解封

## RateLimitConfig字段

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `requests_per_second` | uint32_t | 10 | 每秒允许的请求数 |
| `burst_size` | uint32_t | 20 | 最大突发请求数 |
| `ban_threshold` | uint32_t | 5 | 触发封禁的违规次数 |
| `ban_duration_seconds` | uint32_t | 60 | 封禁时长（秒） |

## 配置方式

### TOML配置

编辑 `/etc/peerlink/peerlink.toml`：

```toml
# 全局限速配置（应用于所有服务）
[rate_limit]
requests_per_second = 10
burst_size = 20
ban_threshold = 5
ban_duration_seconds = 60

# Signaling服务特定限速
[signaling.rate_limit]
requests_per_second = 20
burst_size = 50
ban_threshold = 5
ban_duration_seconds = 300

# DID服务特定限速
[did.rate_limit]
requests_per_second = 10
burst_size = 20
ban_threshold = 5
ban_duration_seconds = 300
```

### 环境变量覆盖

```bash
# 覆盖特定服务的限速配置
export SIGNALING_RATE_LIMIT_RPS=20
export SIGNALING_RATE_LIMIT_BURST=50
export SIGNALING_RATE_LIMIT_BAN_THRESHOLD=5
export SIGNALING_RATE_LIMIT_BAN_DURATION=300
```

### C++代码配置

```cpp
#include "p2p/servers/relay/rate_limiter.hpp"

// 创建配置
p2p::relay::RateLimitConfig config(
    10,    // 10 requests per second
    20,    // burst size
    5,     // ban after 5 violations
    60     // ban for 60 seconds
);

// 创建限速器
p2p::relay::RateLimiter limiter(config);

// 检查请求
if (limiter.AllowRequest("192.168.1.100:12345")) {
    // 处理请求
} else {
    // 拒绝请求
}
```

## 场景推荐配置

### 高流量场景

适用于高并发API服务：

```toml
[rate_limit]
requests_per_second = 50
burst_size = 100
ban_threshold = 10
ban_duration_seconds = 300
```

**特点**：
- 较高的请求限制
- 较大的突发容量
- 较严格的封禁策略

### 严格安全场景

适用于敏感操作（如注册、登录）：

```toml
[rate_limit]
requests_per_second = 5
burst_size = 10
ban_threshold = 3
ban_duration_seconds = 3600
```

**特点**：
- 低请求限制
- 小突发容量
- 快速封禁（3次违规）
- 长封禁时长（1小时）

### 开发测试场景

适用于本地开发环境：

```toml
[rate_limit]
requests_per_second = 1000
burst_size = 2000
ban_threshold = 100
ban_duration_seconds = 10
```

**特点**：
- 几乎无限制
- 便于开发调试

### relay-tunnel场景

适用于SSH隧道服务：

```toml
[rate_limit]
requests_per_second = 20
burst_size = 40
ban_threshold = 5
ban_duration_seconds = 60
```

## 监控指标

### Prometheus指标

```bash
# 访问管理接口获取指标
curl http://127.0.0.1:9300/metrics
```

**关键指标**：

| 指标 | 说明 | 告警阈值 |
|------|------|----------|
| `rate_limit_total_requests` | 总请求数 | - |
| `rate_limit_blocked_requests` | 被拦截请求数 | - |
| `rate_limit_block_rate` | 拦截率 | >50% |
| `rate_limit_banned_clients` | 当前封禁客户端数 | >100 |
| `rate_limit_active_clients` | 活跃客户端数 | - |

### 计算拦截率

```bash
# 使用curl和jq
BLOCKED=$(curl -s http://127.0.0.1:9300/metrics | grep rate_limit_blocked_requests | awk '{print $2}')
TOTAL=$(curl -s http://127.0.0.1:9300/metrics | grep rate_limit_total_requests | awk '{print $2}')
echo "scale=2; $BLOCKED * 100 / $TOTAL" | bc
```

### 告警规则示例

Prometheus告警规则：

```yaml
groups:
  - name: rate_limit_alerts
    rules:
      - alert: HighBlockRate
        expr: rate_limit_block_rate > 0.5
        for: 5m
        annotations:
          summary: "拦截率过高，可能遭受攻击"
          
      - alert: ManyBannedClients
        expr: rate_limit_banned_clients > 100
        for: 5m
        annotations:
          summary: "大量客户端被封禁"
          
      - alert: TrafficSpike
        expr: rate(rate_limit_total_requests[5m]) > 1000
        annotations:
          summary: "流量异常 spike"
```

## 运维操作

### 手动封禁客户端

```bash
# 使用管理API封禁特定IP
curl -X POST http://127.0.0.1:9300/api/v1/rate_limit/ban \
  -H "Content-Type: application/json" \
  -d '{"client": "192.168.1.100:12345", "duration": 3600}'
```

### 手动解封客户端

```bash
curl -X POST http://127.0.0.1:9300/api/v1/rate_limit/unban \
  -H "Content-Type: application/json" \
  -d '{"client": "192.168.1.100:12345"}'
```

### 查看限速统计

```bash
curl -s http://127.0.0.1:9300/api/v1/rate_limit/stats | jq
```

返回示例：

```json
{
  "total_clients": 1250,
  "banned_clients": 3,
  "total_requests": 150000,
  "blocked_requests": 450,
  "block_rate": 0.003
}
```

### 清理过期数据

```bash
# 触发清理（通常自动执行）
curl -X POST http://127.0.0.1:9300/api/v1/rate_limit/cleanup
```

## 性能特性

### 时间复杂度

- `AllowRequest()`: O(1) 平均
- `IsBanned()`: O(1)
- `CleanupExpired()`: O(n)，n为客户端数量

### 空间复杂度

- O(n)，n为唯一客户端数
- 自动清理不活跃客户端

### 并发特性

- 每客户端独立的mutex
- 最小化锁竞争
- 无锁统计更新

## 故障排查

### 请求被误拦截

1. **检查配置**：

```bash
# 查看当前限速配置
curl -s http://127.0.0.1:9300/api/v1/rate_limit/config | jq
```

2. **调整限速参数**：

```toml
[rate_limit]
requests_per_second = 50  # 增加限制
burst_size = 100          # 增加突发
```

3. **添加白名单**：

```bash
curl -X POST http://127.0.0.1:9300/api/v1/rate_limit/whitelist \
  -H "Content-Type: application/json" \
  -d '{"client": "192.168.1.100"}'
```

### 拦截率过高

1. **检查是否遭受攻击**：

```bash
# 查看被封禁的客户端
curl -s http://127.0.0.1:9300/api/v1/rate_limit/banned | jq
```

2. **分析日志**：

```bash
sudo journalctl -u peerlink-gateway -f | grep "rate limit"
```

3. **临时提高限制**：

```bash
export SIGNALING_RATE_LIMIT_RPS=100
sudo systemctl reload peerlink-gateway
```

## 最佳实践

1. **分服务配置**：不同服务使用不同的限速参数
2. **监控拦截率**：设置告警，及时发现异常
3. **定期审查**：定期检查被封禁的客户端
4. **渐进调整**：从小限制开始，逐步调整到合适值
5. **文档记录**：记录每次调整的原因和效果
