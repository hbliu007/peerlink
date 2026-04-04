#!/bin/bash
# Clean and rebuild script for p2p-cpp project

set -e

# Clean PATH to avoid Conda compiler interference
export PATH=/usr/bin:/bin:/usr/sbin:/sbin:/Library/Developer/CommandLineTools/usr/bin:/opt/homebrew/bin

BUILD_TYPE="${1:-Release}"

echo "Cleaning build directory..."
rm -rf build

echo "Configuring CMake..."
cmake -B build \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DBUILD_EXAMPLES=ON \
    -DBUILD_SHARED_LIBS=OFF

echo "Building..."
cmake --build build -j$(sysctl -n hw.ncpu)

echo "Clean build complete!"
