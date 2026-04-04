#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="${SCRIPT_DIR}/../.."

echo "=== TLS Performance Benchmark ==="

# 颜色输出
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# 检查依赖
if ! command -v ab &> /dev/null; then
    echo "Apache Bench (ab) not found. Install: brew install httpd (macOS) or apt-get install apache2-utils (Linux)"
    exit 1
fi

# 清理函数
cleanup() {
    echo -e "\n${YELLOW}Cleaning up...${NC}"
    if [ ! -z "$SERVER_PID" ]; then
        kill $SERVER_PID 2>/dev/null || true
    fi
}

trap cleanup EXIT

# 1. 启动测试服务器
echo -e "${YELLOW}1. Starting test HTTPS server...${NC}"
cd "${PROJECT_ROOT}"

DID_BIN="${PROJECT_ROOT}/build/src/servers/did/p2p-did-server"
if [ ! -f "$DID_BIN" ]; then
    echo "DID server not found. Please build first."
    exit 1
fi

export DID_TLS_ENABLED=true
export DID_TLS_CERT_PATH="${PROJECT_ROOT}/certs/server.crt"
export DID_TLS_KEY_PATH="${PROJECT_ROOT}/certs/server.key"
export DID_PORT=8444

"$DID_BIN" > /tmp/benchmark_server.log 2>&1 &
SERVER_PID=$!
sleep 3

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "Server failed to start"
    cat /tmp/benchmark_server.log
    exit 1
fi

echo -e "${GREEN}✓ Server started (PID: $SERVER_PID)${NC}"

# 2. HTTPS 吞吐量测试
echo -e "\n${YELLOW}2. HTTPS Throughput Test${NC}"
echo "Testing with 10,000 requests, 100 concurrent connections..."

ab -n 10000 -c 100 -k -q https://localhost:8444/health 2>&1 | tee /tmp/benchmark_https.txt

# 提取关键指标
echo -e "\n${GREEN}=== Key Metrics ===${NC}"
grep "Requests per second" /tmp/benchmark_https.txt
grep "Time per request" /tmp/benchmark_https.txt
grep "Transfer rate" /tmp/benchmark_https.txt

# 3. TLS 握手性能测试
echo -e "\n${YELLOW}3. TLS Handshake Performance${NC}"
echo "Testing TLS 1.3 handshake latency (100 connections)..."

START_TIME=$(date +%s%N)
for i in {1..100}; do
    timeout 2 openssl s_client -connect localhost:8444 -tls1_3 </dev/null >/dev/null 2>&1 || true
done
END_TIME=$(date +%s%N)

ELAPSED_MS=$(( ($END_TIME - $START_TIME) / 1000000 ))
AVG_HANDSHAKE_MS=$(( $ELAPSED_MS / 100 ))

echo -e "${GREEN}Average TLS handshake time: ${AVG_HANDSHAKE_MS}ms${NC}"

# 4. 连接复用性能测试
echo -e "\n${YELLOW}4. Connection Reuse Performance${NC}"
echo "Testing with keep-alive (connection reuse)..."

ab -n 5000 -c 50 -k -q https://localhost:8444/health 2>&1 | grep "Requests per second"

echo "Testing without keep-alive (new connection each time)..."
ab -n 5000 -c 50 -q https://localhost:8444/health 2>&1 | grep "Requests per second"

# 5. 并发连接测试
echo -e "\n${YELLOW}5. Concurrent Connection Test${NC}"
for concurrency in 10 50 100 200; do
    echo "Testing with $concurrency concurrent connections..."
    ab -n 1000 -c $concurrency -k -q https://localhost:8444/health 2>&1 | grep "Requests per second"
done

# 6. 生成报告
echo -e "\n${GREEN}=== Benchmark Complete ===${NC}"
echo "Full results saved to:"
echo "  - /tmp/benchmark_https.txt"
echo "  - /tmp/benchmark_server.log"

# 性能建议
echo -e "\n${YELLOW}Performance Tips:${NC}"
echo "1. Enable session caching for better TLS performance"
echo "2. Use connection pooling in clients"
echo "3. Consider hardware acceleration (AES-NI)"
echo "4. Monitor certificate expiration"

exit 0
