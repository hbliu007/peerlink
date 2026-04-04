# 故障排查

常见问题和解决方案。

---

## 连接问题

### 问题：无法连接到 Signaling 服务器

**症状**：
```
[ERROR] Failed to connect to signaling server: Connection refused
```

**诊断步骤**：

```bash
# 1. 检查服务器地址是否正确
peerlink config show | grep signaling

# 2. 测试网络连通性
curl -v https://peerlink.example.com:8443

# 3. 检查防火墙规则
sudo iptables -L | grep 8443  # Linux
netsh advfirewall firewall show rule name=all | findstr 8443  # Windows

# 4. 检查 DNS 解析
nslookup peerlink.example.com
```

**解决方案**：

1. 确认服务器地址和端口正确
2. 检查防火墙是否允许出站连接到 8443 端口
3. 检查 TLS 证书是否有效
4. 尝试使用 IP 地址代替域名（排除 DNS 问题）

---

### 问题：NAT 穿透失败

**症状**：
```
[WARN] UDP hole punching failed
[WARN] TCP hole punching failed
[INFO] Falling back to relay...
```

**诊断步骤**：

```bash
# 1. 检查 NAT 类型
peerlink detect-nat

# 2. 测试 STUN 服务器
peerlink test stun

# 3. 查看详细日志
peerlink logs --verbose
```

**NAT 类型说明**：

| NAT 类型 | UDP 打洞 | TCP 打洞 | 建议 |
|---------|---------|---------|------|
| Full Cone | ✅ | ✅ | 无需操作 |
| Restricted Cone | ✅ | ✅ | 无需操作 |
| Port-Restricted Cone | ✅ | ⚠️ | 可能需要中继 |
| Symmetric | ❌ | ⚠️ | 使用中继 |

**解决方案**：

1. **Port-Restricted Cone NAT**: 增加 UDP 打洞尝试次数
   ```yaml
   network:
     nat:
       udp_retries: 5
   ```

2. **Symmetric NAT**: 强制使用中继
   ```bash
   peerlink connect <peer-id> --force-relay
   ```

3. **企业防火墙**: 使用 TCP 443 中继
   ```bash
   peerlink connect <peer-id> --relay-tls
   ```

---

### 问题：连接频繁断开

**症状**：
```
[WARN] Connection lost: sess_abc123
[INFO] Reconnecting...
```

**诊断步骤**：

```bash
# 1. 检查网络稳定性
ping -c 100 peerlink.example.com

# 2. 查看连接统计
peerlink stats sess_abc123

# 3. 检查心跳状态
peerlink ping sess_abc123
```

**解决方案**：

1. **调整心跳间隔**：
   ```yaml
   network:
     keepalive:
       interval: 30s  # 缩短心跳间隔
       timeout: 90s   # 延长超时时间
   ```

2. **启用自动重连**：
   ```bash
   peerlink connect <peer-id> --auto-reconnect
   ```

3. **检查 NAT 映射超时**：某些 NAT 设备的映射会过期，需要定期发送流量

---

## 性能问题

### 问题：吞吐量低

**症状**：
- 文件传输速度慢
- 带宽测试结果不理想

**诊断步骤**：

```bash
# 1. 测试连接带宽
peerlink benchmark sess_abc123

# 2. 检查连接类型
peerlink list | grep sess_abc123

# 3. 查看系统资源使用
peerlink stats --resource
```

**解决方案**：

1. **确认是直连还是中继**：中继模式会受限于服务器带宽
   ```bash
   # 如果是 RELAY，尝试强制直连
   peerlink connect <peer-id> --force-direct
   ```

2. **调整缓冲区大小**：
   ```yaml
   performance:
     buffer_size: 262144  # 256KB
   ```

3. **调整 TCP 参数**：
   ```yaml
   performance:
     tcp:
       window_size: 1048576  # 1MB
       no_delay: true
   ```

---

### 问题：延迟高

**症状**：
- 实时应用卡顿
- 延迟 > 100ms

**诊断步骤**：

```bash
# 1. 测试延迟
peerlink ping sess_abc123

# 2. 查看路由追踪
traceroute <remote-ip>

# 3. 检查 NAT 类型
peerlink detect-nat
```

**解决方案**：

1. **优先使用 UDP**：
   ```yaml
   network:
     transport:
       preferred: udp
   ```

2. **启用低延迟模式**：
   ```yaml
   performance:
     low_latency: true
     buffer_size: 16384  # 减小缓冲区
   ```

3. **选择最近的 Relay**：
   ```yaml
   server:
     relay:
       - relay1.example.com:443  # 最近的
       - relay2.example.com:443
   ```

---

## 安全问题

### 问题：证书验证失败

**症状**：
```
[ERROR] TLS handshake failed: certificate verify failed
```

**解决方案**：

1. **临时禁用验证（不推荐）**：
   ```yaml
   security:
     tls:
       verify: none
   ```

2. **添加自定义 CA**：
   ```yaml
   security:
     tls:
       ca_file: /path/to/custom-ca.pem
   ```

3. **更新系统证书**：
   ```bash
   # Ubuntu/Debian
   sudo apt-get update && sudo apt-get install ca-certificates

   # CentOS/RHEL
   sudo yum update ca-certificates
   ```

---

### 问题：认证失败

**症状**：
```
[ERROR] Authentication failed: invalid signature
```

**解决方案**：

1. **检查 DID 身份**：
   ```bash
   peerlink whoami
   ```

2. **重新生成密钥**：
   ```bash
   rm ~/.peerlink/identity.json
   peerlink daemon  # 会自动生成新密钥
   ```

3. **检查时钟同步**：
   ```bash
   # 时间不同步会导致签名验证失败
   sudo ntpdate pool.ntp.org
   ```

---

## 调试技巧

### 启用详细日志

```bash
# 临时启用调试日志
peerlink logs --verbose

# 持续启用
peerlink config set logging.level debug
peerlink daemon restart
```

### 抓包分析

```bash
# TCPdump
sudo tcpdump -i any -w peerlink.pcap port 8443 or port 3478 or port 443

# Wireshark
# 打开 peerlink.pcap 文件进行分析
```

### 性能分析

```bash
# CPU 性能分析
perf record -p $(pidof peerlink-daemon)
perf report

# 内存分析
valgrind --leak-check=full peerlink-daemon
```

---

## 获取帮助

如果以上方法都无法解决问题：

1. **查看日志**：
   ```bash
   peerlink logs --tail 100 > peerlink-debug.log
   ```

2. **收集系统信息**：
   ```bash
   peerlink debug-info > system-info.txt
   ```

3. **提交 Issue**：
   - 附上 `peerlink-debug.log`（删除敏感信息）
   - 附上 `system-info.txt`
   - 描述复现步骤

---

**下一步**: [性能优化](performance.md) · [配置指南](configuration.md)
