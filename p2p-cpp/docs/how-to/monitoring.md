---
title: 监控配置指南
description: 使用Prometheus和Grafana监控PeerLink服务，包括Admin HTTP端点、指标采集和告警规则
tags:
  - monitoring
  - prometheus
  - grafana
  - metrics
  - alerting
---

# 监控配置指南

本指南介绍如何使用Prometheus和Grafana监控PeerLink服务的健康状态和性能指标。

## Admin HTTP端点

### 端点列表

所有服务提供以下管理端点：

| 端点 | 方法 | 说明 |
|------|------|------|
| `/healthz` | GET | 服务存活检查 |
| `/readyz` | GET | 服务就绪检查 |
| `/status` | GET | 服务状态和配置快照 |
| `/metrics` | GET | Prometheus文本格式指标 |

### 默认端口

| 服务 | 端口 | 用途 |
|------|------|------|
| Gateway | 9300 | 网关管理接口 |
| Network | 9301 | 网络服务管理接口 |
| Signaling | 9302 | 信令服务管理接口 |
| DID | 9303 | DID服务管理接口 |

### 健康检查

```bash
# 检查Gateway健康
curl http://127.0.0.1:9300/healthz

# 检查Network就绪状态
curl http://127.0.0.1:9301/readyz

# 检查Signaling状态
curl http://127.0.0.1:9302/status

# 获取DID指标
curl http://127.0.0.1:9303/metrics
```

## Prometheus配置

### 安装Prometheus

```bash
# Ubuntu/Debian
sudo apt-get install prometheus

# 或使用Docker
docker run -d \
  --name prometheus \
  -p 9090:9090 \
  -v /path/to/prometheus.yml:/etc/prometheus/prometheus.yml \
  prom/prometheus
```

### 配置文件

创建 `/etc/prometheus/prometheus.yml`：

```yaml
global:
  scrape_interval: 15s
  evaluation_interval: 15s

scrape_configs:
  # PeerLink Gateway
  - job_name: 'peerlink-gateway'
    static_configs:
      - targets: ['localhost:9300']

  # PeerLink Network
  - job_name: 'peerlink-network'
    static_configs:
      - targets: ['localhost:9301']

  # PeerLink Signaling
  - job_name: 'peerlink-signaling'
    static_configs:
      - targets: ['localhost:9302']

  # PeerLink DID
  - job_name: 'peerlink-did'
    static_configs:
      - targets: ['localhost:9303']

  # Node Exporter (系统指标)
  - job_name: 'node'
    static_configs:
      - targets: ['localhost:9100']
```

### 启动Prometheus

```bash
# 测试配置
promtool check config /etc/prometheus/prometheus.yml

# 启动服务
sudo systemctl start prometheus
sudo systemctl enable prometheus

# 访问UI
http://localhost:9090
```

## 可用指标

### 连接指标

```
# 当前连接数
p2p_connections_total{service="signaling"}
p2p_connections_active{service="signaling"}

# P2P直连连接
p2p_connections_p2p{service="signaling"}

# Relay中继连接
p2p_connections_relay{service="signaling"}
```

### 性能指标

```
# 请求延迟
p2p_request_duration_seconds{service="signaling",quantile="0.99"}

# 消息吞吐量
p2p_messages_sent_total{service="signaling"}
p2p_messages_received_total{service="signaling"}

# 带宽使用
p2p_bytes_sent_total{service="signaling"}
p2p_bytes_received_total{service="signaling"}
```

### 限速指标

```
# 限速统计
rate_limit_total_requests{service="signaling"}
rate_limit_blocked_requests{service="signaling"}
rate_limit_block_rate{service="signaling"}
rate_limit_banned_clients{service="signaling"}
rate_limit_active_clients{service="signaling"}
```

### TURN中继指标

```
# 分配统计
turn_allocations_total
turn_allocations_active
turn_allocations_failed_total

# 带宽限制
turn_bandwidth_limit_bytes{direction="send|recv"}
turn_bandwidth_used_bytes{direction="send|recv"}
```

### 系统指标

```
# 进程状态
process_cpu_seconds_total
process_open_fds
process_max_fds

# 内存使用
process_resident_memory_bytes
process_virtual_memory_bytes
```

## Grafana仪表板

### 安装Grafana

```bash
# Ubuntu/Debian
sudo apt-get install grafana

# 或使用Docker
docker run -d \
  --name grafana \
  -p 3000:3000 \
  grafana/grafana
```

### 配置数据源

1. 登录Grafana（默认：http://localhost:3000，admin/admin）
2. 添加Prometheus数据源：
   - URL: `http://localhost:9090`
   - Access: Server (default)

### 导入仪表板

创建JSON文件 `peerlink-dashboard.json`：

```json
{
  "dashboard": {
    "title": "PeerLink Overview",
    "panels": [
      {
        "title": "Active Connections",
        "targets": [
          {
            "expr": "p2p_connections_active"
          }
        ],
        "type": "graph"
      },
      {
        "title": "P2P vs Relay",
        "targets": [
          {
            "expr": "p2p_connections_p2p"
          },
          {
            "expr": "p2p_connections_relay"
          }
        ],
        "type": "graph"
      },
      {
        "title": "Request Rate",
        "targets": [
          {
            "expr": "rate(p2p_messages_sent_total[5m])"
          }
        ],
        "type": "graph"
      },
      {
        "title": "Rate Limit Block Rate",
        "targets": [
          {
            "expr": "rate_limit_block_rate"
          }
        ],
        "type": "gauge"
      },
      {
        "title": "TURN Allocations",
        "targets": [
          {
            "expr": "turn_allocations_active"
          }
        ],
        "type": "graph"
      }
    ]
  }
}
```

### 推荐面板

#### 概览面板

- **Active Connections**: 当前活跃连接数
- **P2P vs Relay**: 直连和中继连接比例
- **Request Rate**: 每秒请求数
- **Block Rate**: 限速拦截率

#### 性能面板

- **Message Throughput**: 消息吞吐量
- **Bandwidth Usage**: 带宽使用情况
- **Latency**: 请求延迟分布
- **Error Rate**: 错误率

#### 资源面板

- **CPU Usage**: CPU使用率
- **Memory Usage**: 内存使用量
- **File Descriptors**: 文件描述符使用
- **Network I/O**: 网络IO

## 告警规则

### 配置告警规则

创建 `/etc/prometheus/rules/peerlink.yml`：

```yaml
groups:
  - name: peerlink_alerts
    interval: 30s
    rules:
      # 服务可用性告警
      - alert: PeerLinkServiceDown
        expr: up{job=~"peerlink-.*"} == 0
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "PeerLink服务 {{ $labels.instance }} 宕机"
          description: "服务 {{ $labels.job }} 已宕机超过1分钟"

      # 高拦截率告警
      - alert: HighBlockRate
        expr: rate_limit_block_rate > 0.5
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "服务 {{ $labels.instance }} 拦截率过高"
          description: "拦截率 {{ $value }} 超过50%，可能遭受攻击"

      # 大量封禁告警
      - alert: ManyBannedClients
        expr: rate_limit_banned_clients > 100
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "服务 {{ $labels.instance }} 大量客户端被封禁"
          description: "封禁客户端数: {{ $value }}"

      # 连接数异常告警
      - alert: ConnectionDrop
        expr: rate(p2p_connections_total[5m]) < 0
        for: 2m
        labels:
          severity: warning
        annotations:
          summary: "服务 {{ $labels.instance }} 连接数下降"
          description: "连接数在持续减少"

      # Relay比例过高告警
      - alert: HighRelayRatio
        expr: |
          (
            p2p_connections_relay /
            (p2p_connections_p2p + p2p_connections_relay)
          ) > 0.8
        for: 10m
        labels:
          severity: info
        annotations:
          summary: "服务 {{ $labels.instance }} Relay比例过高"
          description: "超过80%的连接使用中继，可能网络环境不佳"

      # TURN分配失败告警
      - alert: TURNAllocationFailures
        expr: rate(turn_allocations_failed_total[5m]) > 10
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "TURN分配失败率过高"
          description: "每秒超过10个TURN分配失败"

      # 内存使用告警
      - alert: HighMemoryUsage
        expr: |
          (
            process_resident_memory_bytes{job=~"peerlink-.*"} /
            process_virtual_memory_bytes{job=~"peerlink-.*"}
          ) > 0.9
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "服务 {{ $labels.instance }} 内存使用率过高"
          description: "内存使用率: {{ $value }}%"
```

### 加载告警规则

在 `/etc/prometheus/prometheus.yml` 中添加：

```yaml
rule_files:
  - "/etc/prometheus/rules/*.yml"
```

重载配置：

```bash
sudo systemctl reload prometheus
```

### 验证告警

访问Prometheus UI查看告警状态：

```
http://localhost:9090/alerts
```

## 告警通知

### 配置Alertmanager

创建 `/etc/prometheus/alertmanager.yml`：

```yaml
global:
  resolve_timeout: 5m

route:
  group_by: ['alertname', 'instance']
  group_wait: 10s
  group_interval: 10s
  repeat_interval: 12h
  receiver: 'email'

  routes:
    - match:
        severity: critical
      receiver: 'email-critical'
      group_wait: 10s
      repeat_interval: 5m

receivers:
  - name: 'email'
    email_configs:
      - to: 'alerts@example.com'
        from: 'prometheus@example.com'
        smarthost: 'smtp.example.com:587'
        auth_username: 'prometheus@example.com'
        auth_password: 'password'

  - name: 'email-critical'
    email_configs:
      - to: 'oncall@example.com'
        from: 'prometheus@example.com'
        smarthost: 'smtp.example.com:587'
```

### 启动Alertmanager

```bash
# 使用Docker
docker run -d \
  --name alertmanager \
  -p 9093:9093 \
  -v /etc/prometheus/alertmanager.yml:/etc/alertmanager/alertmanager.yml \
  prom/alertmanager
```

### 配置Prometheus使用Alertmanager

在 `/etc/prometheus/prometheus.yml` 中添加：

```yaml
alerting:
  alertmanagers:
    - static_configs:
        - targets: ['localhost:9093']
```

## 常用查询示例

### PromQL查询

```promql
# 过去5分钟的请求速率
rate(p2p_messages_sent_total[5m])

# 99分位延迟
histogram_quantile(0.99, rate(p2p_request_duration_seconds_bucket[5m]))

# P2P连接比例
p2p_connections_p2p / (p2p_connections_p2p + p2p_connections_relay)

# 按服务分组的活跃连接
sum by (service) (p2p_connections_active)

# 预测未来1小时的趋势
predict_linear(p2p_connections_active[1h], 3600)
```

## 最佳实践

1. **合理设置采样间隔**：生产环境建议15-30秒
2. **保留历史数据**：配置`storage.tsdb.retention.time`
3. **告警分级**：使用critical/warning/info区分严重程度
4. **定期审查**：定期审查和优化告警规则
5. **测试告警**：定期测试告警是否正常工作
