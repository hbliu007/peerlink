#!/bin/bash
set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Default values
BUILD_TYPE="Release"
BUILD_DIR="build"
JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
ENABLE_TESTS=ON
ENABLE_EXAMPLES=ON
ENABLE_SERVERS=ON
ENABLE_PYTHON=OFF
ENABLE_ASAN=OFF
ENABLE_COVERAGE=OFF

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --release)
            BUILD_TYPE="Release"
            shift
            ;;
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --jobs)
            JOBS="$2"
            shift 2
            ;;
        --no-tests)
            ENABLE_TESTS=OFF
            shift
            ;;
        --no-examples)
            ENABLE_EXAMPLES=OFF
            shift
            ;;
        --no-servers)
            ENABLE_SERVERS=OFF
            shift
            ;;
        --python)
            ENABLE_PYTHON=ON
            shift
            ;;
        --asan)
            ENABLE_ASAN=ON
            BUILD_TYPE="Debug"
            shift
            ;;
        --coverage)
            ENABLE_COVERAGE=ON
            BUILD_TYPE="Debug"
            shift
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --debug           Build in Debug mode (default: Release)"
            echo "  --release         Build in Release mode"
            echo "  --build-dir DIR   Build directory (default: build)"
            echo "  --jobs N          Number of parallel jobs (default: auto)"
            echo "  --no-tests        Disable tests"
            echo "  --no-examples     Disable examples"
            echo "  --no-servers      Disable server components"
            echo "  --python          Enable Python bindings"
            echo "  --asan            Enable AddressSanitizer (implies --debug)"
            echo "  --coverage        Enable code coverage (implies --debug)"
            echo "  --help            Show this help message"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            exit 1
            ;;
    esac
done

echo -e "${GREEN}=== P2P Platform C++ Build Script ===${NC}"
echo ""
echo "Build Type:       $BUILD_TYPE"
echo "Build Directory:  $BUILD_DIR"
echo "Parallel Jobs:    $JOBS"
echo "Tests:            $ENABLE_TESTS"
echo "Examples:         $ENABLE_EXAMPLES"
echo "Servers:          $ENABLE_SERVERS"
echo "Python Bindings:  $ENABLE_PYTHON"
echo "AddressSanitizer: $ENABLE_ASAN"
echo "Code Coverage:    $ENABLE_COVERAGE"
echo ""

# Check dependencies
echo -e "${YELLOW}Checking dependencies...${NC}"

check_command() {
    if ! command -v $1 &> /dev/null; then
        echo -e "${RED}Error: $1 is not installed${NC}"
        exit 1
    fi
}

check_command cmake
check_command make

echo -e "${GREEN}All dependencies found${NC}"
echo ""

# Configure
echo -e "${YELLOW}Configuring...${NC}"
cmake -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DBUILD_TESTS="$ENABLE_TESTS" \
    -DBUILD_EXAMPLES="$ENABLE_EXAMPLES" \
    -DBUILD_SERVERS="$ENABLE_SERVERS" \
    -DBUILD_BINDINGS_PYTHON="$ENABLE_PYTHON" \
    -DENABLE_ASAN="$ENABLE_ASAN" \
    -DENABLE_COVERAGE="$ENABLE_COVERAGE"

echo ""

# Build
echo -e "${YELLOW}Building...${NC}"
cmake --build "$BUILD_DIR" -j"$JOBS"

echo ""
echo -e "${GREEN}Build completed successfully!${NC}"
echo ""
echo "Next steps:"
echo "  - Run tests:    cd $BUILD_DIR && ctest -V"
echo "  - Install:      sudo cmake --install $BUILD_DIR"
echo "  - Run examples: ./$BUILD_DIR/examples/basic/simple_client"