#!/bin/bash

set -e

echo "=== DID Service Integration Tests ==="
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

echo "Using externally managed native DID service and Redis."
echo "Override endpoints with PEERLINK_TEST_DID_SERVICE_URL, PEERLINK_TEST_REDIS_HOST, and PEERLINK_TEST_REDIS_PORT."

# Run tests
echo
echo "Running DID integration tests..."
echo

pytest scenarios/ -v --tb=short --timeout=60

# Capture exit code
TEST_EXIT_CODE=$?

# Deactivate venv
deactivate

if [ $TEST_EXIT_CODE -eq 0 ]; then
    echo
    echo "✅ All DID integration tests passed!"
else
    echo
    echo "❌ Some tests failed"
fi

exit $TEST_EXIT_CODE
