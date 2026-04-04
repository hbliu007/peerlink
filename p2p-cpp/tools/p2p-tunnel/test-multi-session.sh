#!/bin/bash
# Test script for multi-session relay tunnel

echo "=== Multi-Session Relay Tunnel Test ==="
echo ""
echo "This script will test multiple concurrent SSH sessions through the tunnel."
echo ""
echo "Prerequisites:"
echo "  1. Server running on office computer:"
echo "     ./relay-tunnel server --did office-213 --relay <YOUR_SERVER_IP>:9443 --forward 127.0.0.1:22"
echo ""
echo "  2. Client running on this computer:"
echo "     ./relay-tunnel client --did home-mac --target office-213 --relay <YOUR_SERVER_IP>:9443 --listen 9022"
echo ""
echo "Testing multiple SSH sessions..."
echo ""

# Test session 1
echo "Opening SSH session 1..."
ssh -p 9022 -o ConnectTimeout=5 <USER>@127.0.0.1 "echo 'Session 1 connected'; hostname; date" &
PID1=$!

sleep 2

# Test session 2
echo "Opening SSH session 2..."
ssh -p 9022 -o ConnectTimeout=5 <USER>@127.0.0.1 "echo 'Session 2 connected'; hostname; date" &
PID2=$!

sleep 2

# Test session 3
echo "Opening SSH session 3..."
ssh -p 9022 -o ConnectTimeout=5 <USER>@127.0.0.1 "echo 'Session 3 connected'; hostname; date" &
PID3=$!

# Wait for all sessions
wait $PID1 $PID2 $PID3

echo ""
echo "=== Test Complete ==="
echo "If all 3 sessions connected successfully, multi-session support is working!"
