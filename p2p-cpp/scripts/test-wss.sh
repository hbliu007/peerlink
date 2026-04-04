#!/bin/bash
# 测试 WSS (WebSocket over TLS) 连接

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=== WSS Connection Test ==="
echo ""

# 检查 wscat 是否安装
if ! command -v wscat &> /dev/null; then
    echo "Error: wscat not found. Install it with:"
    echo "  npm install -g wscat"
    exit 1
fi

# 检查证书是否存在
CERT_DIR="$PROJECT_ROOT/certs"
if [ ! -f "$CERT_DIR/server.crt" ] || [ ! -f "$CERT_DIR/server.key" ]; then
    echo "Error: TLS certificates not found in $CERT_DIR"
    echo "Generate them with:"
    echo "  ./scripts/generate-dev-cert.sh"
    exit 1
fi

# 测试参数
HOST="${1:-localhost}"
PORT="${2:-8443}"

echo "Testing WSS connection to $HOST:$PORT"
echo ""

# 测试 WSS 连接（自签名证书需要 --no-check）
echo "Connecting to wss://$HOST:$PORT (press Ctrl+C to exit)..."
echo ""
echo "Expected behavior:"
echo "  1. SSL handshake should complete"
echo "  2. WebSocket handshake should complete"
echo "  3. Server should send 'registered' message"
echo ""

wscat -c "wss://$HOST:$PORT" --no-check
