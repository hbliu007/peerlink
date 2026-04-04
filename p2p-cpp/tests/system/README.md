# P2P Platform System Tests

End-to-end system tests for the P2P platform.

## Test Scenarios

### 1. Complete P2P Connection (`test_p2p_connection.py`)
- DID device registration (2 devices)
- STUN NAT traversal
- Signaling exchange
- P2P connection establishment
- Data transfer

### 2. Relay Fallback (`test_relay_fallback.py`)
- NAT traversal failure simulation
- Automatic relay fallback
- Data transfer through TURN relay
- Concurrent relay allocations

### 3. Fault Recovery (`test_fault_recovery.py`)
- Device offline/reconnection
- Signaling server restart
- Redis connection recovery

### 4. Performance Tests (`test_performance.py`)
- 1000 device registrations
- 100 concurrent connections
- STUN throughput test
- Rate limiting verification

## Prerequisites

- Native PeerLink services already running
- Python 3.8+
- 8GB RAM minimum
- Reachable service endpoints for signaling, DID, STUN, relay, and Redis
- When signaling secure registration is enabled, export `PEERLINK_TEST_JWT_SECRET`

## Running Tests

### Quick Start

```bash
cd tests/system
./run_tests.sh
```

### Manual Execution

```bash
# 1. Install dependencies
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt

# 2. Export endpoints when defaults do not match your host
export PEERLINK_TEST_SIGNALING_HOST=127.0.0.1
export PEERLINK_TEST_SIGNALING_PORT=8080
export PEERLINK_TEST_DID_HOST=127.0.0.1
export PEERLINK_TEST_DID_PORT=8081
export PEERLINK_TEST_RELAY_PORT=9001
export PEERLINK_TEST_JWT_SECRET=replace-with-your-test-secret

# Optional restart hooks for fault-recovery tests
export PEERLINK_RESTART_SIGNALING_CMD="sudo systemctl restart peerlink-gateway"
export PEERLINK_RESTART_REDIS_CMD="sudo systemctl restart redis-server"

# 3. Run tests
pytest scenarios/ -v
```

### Run Specific Scenarios

```bash
# P2P connection tests only
pytest scenarios/test_p2p_connection.py -v

# Performance tests only
pytest scenarios/test_performance.py -v

# With detailed output
pytest scenarios/ -v -s
```

## Test Results

Expected results:
- **P2P Connection**: 4/4 tests pass
- **Relay Fallback**: 3/3 tests pass
- **Fault Recovery**: 3/3 tests pass
- **Performance**: 4/4 tests pass

Total: **14 tests**, ~5-10 minutes runtime

## Architecture

```
tests/system/
├── conftest.py             # Native service endpoint fixtures
├── requirements.txt        # Python dependencies
├── run_tests.sh           # Test runner
├── scenarios/             # Test scenarios
│   ├── test_p2p_connection.py
│   ├── test_relay_fallback.py
│   ├── test_fault_recovery.py
│   └── test_performance.py
└── utils/                 # Test utilities
    └── test_clients.py    # Client implementations
```

## Troubleshooting

### Services not reachable
```bash
systemctl status peerlink-gateway
systemctl status peerlink-network
ss -lntup | rg '8080|8081|3478|3479|6379'
```

### Port conflicts
```bash
# Check ports
lsof -i :3478
lsof -i :8080

# Override ports with PEERLINK_TEST_* environment variables
```

### Test timeouts
```bash
# Increase timeout
pytest scenarios/ --timeout=300
```

## Notes

- These tests no longer manage services for you.
- Default local-native ports are `8080/8081/3478/9001/6379`; override with `PEERLINK_TEST_*` when targeting Alibaba Cloud or a non-default host.
- Fault-recovery scenarios require restart commands to be provided via environment variables.
- Docker is intentionally not part of this test path.

## Performance Benchmarks

Expected performance (on 4-core, 8GB RAM):
- Device registration: >500/sec
- Concurrent connections: 100 in <10s
- STUN throughput: >1000 req/sec
- Relay allocation: >50/sec
