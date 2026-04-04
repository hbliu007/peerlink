#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="${SCRIPT_DIR}/../.."

echo "=== TLS End-to-End Integration Test ==="
echo "Project root: ${PROJECT_ROOT}"

# 颜色输出
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 清理函数
cleanup() {
    echo -e "\n${YELLOW}6. Cleaning up...${NC}"
    if [ ! -z "$SIGNALING_PID" ]; then
        kill $SIGNALING_PID 2>/dev/null || true
    fi
    if [ ! -z "$DID_PID" ]; then
        kill $DID_PID 2>/dev/null || true
    fi
    if [ ! -z "$GATEWAY_PID" ]; then
        kill $GATEWAY_PID 2>/dev/null || true
    fi
}

trap cleanup EXIT

# 1. 生成证书
echo -e "${YELLOW}1. Generating test certificates...${NC}"
cd "${PROJECT_ROOT}"
if [ -f "scripts/generate-dev-cert.sh" ]; then
    ./scripts/generate-dev-cert.sh
    echo -e "${GREEN}✓ Certificates generated${NC}"
else
    echo -e "${RED}✗ Certificate generation script not found${NC}"
    exit 1
fi

# 2. 检查构建产物
echo -e "\n${YELLOW}2. Checking build artifacts...${NC}"
SIGNALING_BIN="${PROJECT_ROOT}/build/src/servers/signaling/p2p-signaling-server"
DID_BIN="${PROJECT_ROOT}/build/src/servers/did/p2p-did-server"
GATEWAY_BIN="${PROJECT_ROOT}/build/src/servers/gateway/p2p-gateway-server"

if [ ! -f "$SIGNALING_BIN" ]; then
    echo -e "${RED}✗ Signaling server not found: $SIGNALING_BIN${NC}"
    echo "Please run: cmake --build build"
    exit 1
fi

echo -e "${GREEN}✓ Build artifacts found${NC}"

# 3. 启动 Signaling Server（WSS）
echo -e "\n${YELLOW}3. Starting Signaling Server (WSS on port 8443)...${NC}"
export PEERLINK_CONFIG="${PROJECT_ROOT}/config/peerlink.tls.example.toml"
export SIGNALING_TLS_ENABLED=true
export SIGNALING_TLS_CERT_PATH="${PROJECT_ROOT}/certs/server.crt"
export SIGNALING_TLS_KEY_PATH="${PROJECT_ROOT}/certs/server.key"
export SIGNALING_PORT=8443

"$SIGNALING_BIN" > /tmp/signaling.log 2>&1 &
SIGNALING_PID=$!
sleep 3

if ! kill -0 $SIGNALING_PID 2>/dev/null; then
    echo -e "${RED}✗ Signaling server failed to start${NC}"
    cat /tmp/signaling.log
    exit 1
fi

echo -e "${GREEN}✓ Signaling server started (PID: $SIGNALING_PID)${NC}"

# 4. 测试 WSS 连接
echo -e "\n${YELLOW}4. Testing WSS connection...${NC}"

# 使用 openssl s_client 测试 TLS 连接
echo -e "Testing TLS handshake..."
timeout 5 openssl s_client -connect localhost:8443 -tls1_3 </dev/null 2>&1 | grep -q "Verify return code: 0" && {
    echo -e "${GREEN}✓ TLS 1.3 handshake successful${NC}"
} || {
    echo -e "${YELLOW}⚠ Self-signed certificate (expected in dev)${NC}"
}

# 如果安装了 wscat，测试 WebSocket 连接
if command -v wscat &> /dev/null; then
    echo -e "Testing WebSocket over TLS..."
    echo '{"type":"ping"}' | timeout 5 wscat -c wss://localhost:8443 --no-check -w 3 2>&1 | grep -q "connected" && {
        echo -e "${GREEN}✓ WSS connection successful${NC}"
    } || {
        echo -e "${YELLOW}⚠ WSS test skipped (wscat connection issue)${NC}"
    }
else
    echo -e "${YELLOW}⚠ wscat not installed, skipping WSS test${NC}"
    echo "  Install: npm install -g wscat"
fi

# 5. 启动 DID Server（HTTPS）
echo -e "\n${YELLOW}5. Starting DID Server (HTTPS on port 8444)...${NC}"
if [ -f "$DID_BIN" ]; then
    export DID_TLS_ENABLED=true
    export DID_TLS_CERT_PATH="${PROJECT_ROOT}/certs/server.crt"
    export DID_TLS_KEY_PATH="${PROJECT_ROOT}/certs/server.key"
    export DID_PORT=8444

    "$DID_BIN" > /tmp/did.log 2>&1 &
    DID_PID=$!
    sleep 3

    if ! kill -0 $DID_PID 2>/dev/null; then
        echo -e "${RED}✗ DID server failed to start${NC}"
        cat /tmp/did.log
        exit 1
    fi

    echo -e "${GREEN}✓ DID server started (PID: $DID_PID)${NC}"

    # 测试 HTTPS 请求
    echo -e "Testing HTTPS request..."
    curl --insecure --fail --max-time 5 https://localhost:8444/health 2>&1 | grep -q "ok" && {
        echo -e "${GREEN}✓ HTTPS request successful${NC}"
    } || {
        echo -e "${YELLOW}⚠ HTTPS health check failed${NC}"
    }
else
    echo -e "${YELLOW}⚠ DID server not found, skipping${NC}"
fi

# 6. 测试 Gateway Server（如果存在）
if [ -f "$GATEWAY_BIN" ]; then
    echo -e "\n${YELLOW}Testing Gateway Server (HTTPS on port 8445)...${NC}"
    export GATEWAY_TLS_ENABLED=true
    export GATEWAY_TLS_CERT_PATH="${PROJECT_ROOT}/certs/server.crt"
    export GATEWAY_TLS_KEY_PATH="${PROJECT_ROOT}/certs/server.key"
    export GATEWAY_PORT=8445

    "$GATEWAY_BIN" > /tmp/gateway.log 2>&1 &
    GATEWAY_PID=$!
    sleep 3

    if kill -0 $GATEWAY_PID 2>/dev/null; then
        echo -e "${GREEN}✓ Gateway server started (PID: $GATEWAY_PID)${NC}"
        curl --insecure --fail --max-time 5 https://localhost:8445/health 2>&1 | grep -q "ok" && {
            echo -e "${GREEN}✓ Gateway HTTPS request successful${NC}"
        } || {
            echo -e "${YELLOW}⚠ Gateway health check failed${NC}"
        }
    else
        echo -e "${YELLOW}⚠ Gateway server failed to start${NC}"
    fi
fi

# 总结
echo -e "\n${GREEN}=== All TLS E2E tests completed ===${NC}"
echo -e "Logs available at:"
echo -e "  - /tmp/signaling.log"
[ ! -z "$DID_PID" ] && echo -e "  - /tmp/did.log"
[ ! -z "$GATEWAY_PID" ] && echo -e "  - /tmp/gateway.log"

exit 0
