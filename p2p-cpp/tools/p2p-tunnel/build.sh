#!/bin/bash
# Build script for relay-tunnel with multi-session support

set -e

echo "Building relay-tunnel..."

# Detect OS and set compiler flags
if [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS with Homebrew
    g++ -std=c++17 -O2 \
        -I/opt/homebrew/include \
        -L/opt/homebrew/lib \
        -o relay-tunnel relay_tunnel.cpp \
        -lpthread
else
    # Linux
    g++ -std=c++17 -O2 \
        -o relay-tunnel relay_tunnel.cpp \
        -lpthread -lboost_system
fi

echo "Build successful: $(pwd)/relay-tunnel"
ls -lh relay-tunnel
