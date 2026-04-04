#!/bin/bash

set -e

echo "=== P2P Platform System Tests ==="
echo

if ! command -v python3 &> /dev/null; then
    echo "Error: python3 not found"
    exit 1
fi

# Setup Python environment
echo "Setting up Python environment..."
python3 -m venv .venv
source .venv/bin/activate
pip install -q -r requirements.txt

echo "Using externally managed native services."
echo "Override endpoints with PEERLINK_TEST_* environment variables if needed."
echo "Optional restart hooks:"
echo "  PEERLINK_RESTART_SIGNALING_CMD"
echo "  PEERLINK_RESTART_REDIS_CMD"

# Run tests
echo
echo "Running system tests..."
echo

pytest scenarios/ -v --tb=short --timeout=120

# Capture exit code
TEST_EXIT_CODE=$?

# Deactivate venv
deactivate

if [ $TEST_EXIT_CODE -eq 0 ]; then
    echo
    echo "✅ All system tests passed!"
else
    echo
    echo "❌ Some tests failed"
fi

exit $TEST_EXIT_CODE
