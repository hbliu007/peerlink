#!/bin/bash
# 启动办公机SSH隧道 - Mac端

RELAY_HOST="<YOUR_SERVER_IP>:9443"
TARGET_DID="office-213"
LOCAL_DID="home-mac"
LOCAL_PORT="9022"

echo "=== 启动Relay Tunnel Client ==="
echo "目标: $TARGET_DID"
echo "本地端口: $LOCAL_PORT"
echo ""

cd "$(dirname "$0")"

./relay-tunnel-single client \
  --did "$LOCAL_DID" \
  --target "$TARGET_DID" \
  --relay "$RELAY_HOST" \
  --listen "127.0.0.1:$LOCAL_PORT"
