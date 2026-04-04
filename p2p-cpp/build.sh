#!/bin/bash
# Build script for p2p-cpp project
# Fixes macOS SDK conflict with Conda/Miniforge

set -e

# Clean PATH to avoid Conda compiler interference
export PATH=/usr/bin:/bin:/usr/sbin:/sbin:/Library/Developer/CommandLineTools/usr/bin:/opt/homebrew/bin

# Parse arguments
BUILD_TYPE="${1:-Release}"
TARGET="${2:-all}"

echo "Building p2p-cpp..."
echo "Build Type: $BUILD_TYPE"
echo "Target: $TARGET"

# Configure if build directory doesn't exist
if [ ! -d "build" ]; then
    echo "Configuring CMake..."
    cmake -B build \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DBUILD_EXAMPLES=ON \
        -DBUILD_SHARED_LIBS=OFF
fi

# Build
echo "Building..."
if [ "$TARGET" = "all" ]; then
    cmake --build build -j$(sysctl -n hw.ncpu)
else
    cmake --build build --target "$TARGET" -j$(sysctl -n hw.ncpu)
fi

echo "Build complete!"
