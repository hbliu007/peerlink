# Relay/TURN Server C++ Implementation

## Overview

High-performance TURN (RFC 5766) relay server implementation in C++20 with Asio for P2P data relay.

## Architecture

```
relay-server/
├── turn_message.hpp/cpp      - TURN protocol message parsing/serialization
├── port_pool.hpp/cpp          - Bitmap-based port pool (O(1) allocation)
├── allocation_manager.hpp/cpp - TURN allocation lifecycle management
├── bandwidth_limiter.hpp/cpp  - Token bucket rate limiting
└── relay_server.hpp/cpp       - Main relay server (TODO)
```

## Features Implemented

### ✅ TURN Protocol Messages
- Message types: BINDING, ALLOCATE, REFRESH, CREATE_PERMISSION, SEND, DATA
- Attribute types: XOR-MAPPED-ADDRESS, LIFETIME, ERROR-CODE, etc.
- Message parsing and serialization
- XOR address encoding/decoding

### ✅ Port Pool Management
- Bitmap implementation for O(1) port lookup
- Random port selection for load distribution
- Thread-safe with mutex protection
- Supports up to 16,384 ports

### ✅ Allocation Manager
- TURN allocation lifecycle (create, refresh, delete)
- Permission management per allocation
- Triple indexing: by ID, client address, relay address
- Automatic cleanup of expired allocations
- Statistics tracking (bytes sent/received)

### ✅ Bandwidth Control
- Token bucket rate limiting algorithm
- Global + per-allocation bandwidth limits
- Separate read/write throttling
- Throughput monitoring with sliding window

## Performance Optimizations

1. **Zero-Copy Design**: Uses `std::span` and reference passing
2. **Lock-Free Port Pool**: Bitmap with fine-grained locking
3. **Memory Efficiency**: Smart pointers, object pooling ready
4. **Async I/O**: Asio-based event loop (relay_server.cpp TODO)

## Dependencies

- **C++20**: Modern C++ features
- **Asio**: Async I/O (standalone or Boost)
- **OpenSSL**: For future DTLS support
- **spdlog**: High-performance logging
- **GoogleTest**: Unit testing

## Building

```bash
cd p2p-cpp
mkdir build && cd build
cmake .. -DBUILD_SERVERS=ON -DBUILD_TESTS=ON
cmake --build . -j$(nproc)
```

## Testing

```bash
# Run unit tests
./build/tests/servers/relay/test_port_pool
./build/tests/servers/relay/test_allocation
./build/tests/servers/relay/test_bandwidth
```

## TODO

### High Priority
- [ ] `relay_server.cpp` - Main server implementation with Asio
- [ ] `main.cpp` - Server entry point
- [ ] Complete unit tests for all components
- [ ] Integration tests with Python client

### Medium Priority
- [ ] TCP transport support (currently UDP only)
- [ ] Channel binding (RFC 5766 Section 11)
- [ ] DTLS support for secure relay
- [ ] Metrics and monitoring

### Low Priority
- [ ] IPv6 support
- [ ] QUIC transport
- [ ] Connection pooling optimization

## Performance Targets

| Metric | Target | Python Baseline |
|--------|--------|-----------------|
| Concurrent Allocations | 10,000+ | 500+ |
| Throughput | 1 Gbps+ | ~15 Mbps |
| Latency | <1ms | ~5ms |
| Memory per Allocation | <1KB | ~10KB |

## API Example

```cpp
#include "p2p/servers/relay/relay_server.hpp"

using namespace p2p::relay;

int main() {
    RelayServerConfig config;
    config.host = "0.0.0.0";
    config.port = 9001;
    config.public_ip = "203.0.113.1";
    config.min_port = 50000;
    config.max_port = 50100;

    RelayServer server(config);
    server.Start();

    // Server runs until stopped
    std::this_thread::sleep_for(std::chrono::hours(24));

    server.Stop();
    return 0;
}
```

## Protocol Compliance

- ✅ RFC 5389 (STUN)
- ✅ RFC 5766 (TURN) - Core features
- ⏳ RFC 6062 (TURN TCP) - TODO
- ⏳ RFC 7350 (TURN IPv6) - TODO

## License

MIT License - See LICENSE file

## Contributors

- P2P Platform Team
- Relay Server Engineer

## References

- [RFC 5766 - TURN](https://tools.ietf.org/html/rfc5766)
- [RFC 5389 - STUN](https://tools.ietf.org/html/rfc5389)
- [Asio Documentation](https://think-async.com/Asio/)
