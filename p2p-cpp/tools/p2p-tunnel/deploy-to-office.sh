#!/bin/bash
# Deploy updated relay-tunnel to office computer

OFFICE_HOST="${OFFICE_HOST:?Set OFFICE_HOST env var}"
OFFICE_DIR="/home/<USER>/p2p-tunnel"

echo "=== Deploying Multi-Session Relay Tunnel to Office Computer ==="
echo ""

# Check if we can connect
if ! ssh -o ConnectTimeout=5 $OFFICE_HOST "echo 'Connected'" 2>/dev/null; then
    echo "ERROR: Cannot connect to office computer at $OFFICE_HOST"
    echo ""
    echo "Please ensure:"
    echo "  1. You are on the office network or VPN"
    echo "  2. The office computer is powered on"
    echo "  3. SSH is accessible"
    echo ""
    echo "Alternative: Manually copy these files to the office computer:"
    echo "  - relay_tunnel.cpp"
    echo "  - build.sh"
    echo "  - MULTI_SESSION_README.md"
    exit 1
fi

echo "Creating directory on office computer..."
ssh $OFFICE_HOST "mkdir -p $OFFICE_DIR"

echo "Copying files..."
scp relay_tunnel.cpp build.sh MULTI_SESSION_README.md $OFFICE_HOST:$OFFICE_DIR/

echo "Building on office computer..."
ssh $OFFICE_HOST "cd $OFFICE_DIR && chmod +x build.sh && ./build.sh"

echo ""
echo "=== Deployment Complete ==="
echo ""
echo "To start the server on office computer:"
echo "  ssh $OFFICE_HOST"
echo "  cd $OFFICE_DIR"
echo "  ./relay-tunnel server --did office-213 --relay <YOUR_SERVER_IP>:9443 --forward 127.0.0.1:22"
echo ""
echo "Then start the client on this computer:"
echo "  ./relay-tunnel client --did home-mac --target office-213 --relay <YOUR_SERVER_IP>:9443 --listen 9022"
