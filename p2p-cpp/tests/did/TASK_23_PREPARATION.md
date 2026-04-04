# Task #23: DID Service Unit Tests - Preparation

## 📋 Task Overview

**Assigned to**: engineer-stun
**Priority**: High (P2)
**Estimated Time**: 4-6 hours
**Dependencies**: Task #27 (DID Core) and #24 (DID API)
**Target Coverage**: ≥ 80% (Core modules ≥ 90%)

## 🎯 Test Modules to Implement

### 1. test_crypto.cpp - Cryptography Tests
**Coverage Target**: ≥ 90%

Test Cases:
- ✅ Ed25519 key pair generation
- ✅ Key serialization (PEM format)
- ✅ Signature creation and verification
- ✅ Invalid signature rejection
- ✅ DID generation from public key
- ✅ DID format validation
- ✅ Key encoding/decoding

**Reference**: Python test lines 55-114

### 2. test_storage.cpp - Storage Layer Tests
**Coverage Target**: ≥ 90%

Test Cases:
- ✅ Device registration
- ✅ Device query by DID
- ✅ Heartbeat update
- ✅ Status management (online/offline)
- ✅ Query online devices
- ✅ Query by device type
- ✅ Device deletion
- ✅ Expired device cleanup
- ✅ Batch operations

**Reference**: Python test lines 161-224, 361-398

### 3. test_auth.cpp - Authentication Tests
**Coverage Target**: ≥ 90%

Test Cases:
- ✅ Challenge creation
- ✅ Challenge-response verification
- ✅ Token generation
- ✅ Token validation
- ✅ Token expiration
- ✅ Invalid token rejection
- ✅ Replay attack prevention

**Reference**: Python test lines 226-275, 313-359

### 4. test_rate_limiter.cpp - Rate Limiting Tests
**Coverage Target**: ≥ 85%

Test Cases:
- ✅ Per-minute limit (60 req/min)
- ✅ Per-hour limit (1000 req/hour)
- ✅ Burst limit (10 req/sec)
- ✅ Client blocking
- ✅ Client state cleanup
- ✅ Multiple clients isolation
- ✅ Rate limit reset

**Reference**: New implementation (no Python equivalent)

### 5. test_validators.cpp - Input Validation Tests
**Coverage Target**: ≥ 85%

Test Cases:
- ✅ DID format validation
- ✅ Signature format validation
- ✅ Challenge format validation
- ✅ Metadata validation
- ✅ Capability list validation
- ✅ Input sanitization
- ✅ SQL injection prevention
- ✅ XSS prevention

**Reference**: Python test lines 16-53

## 🛠️ Testing Framework

### Google Test Setup
```cmake
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})
```

### Google Mock for Redis
```cpp
class MockRedisClient {
public:
    MOCK_METHOD(bool, set, (const std::string&, const std::string&));
    MOCK_METHOD(std::optional<std::string>, get, (const std::string&));
    MOCK_METHOD(bool, del, (const std::string&));
    MOCK_METHOD(bool, expire, (const std::string&, int));
};
```

## 📊 Test Data Fixtures

### Sample DID Document
```json
{
  "@context": ["https://www.w3.org/ns/did/v1"],
  "id": "did:p2p:device123",
  "verificationMethod": [{
    "id": "did:p2p:device123#key-1",
    "type": "Ed25519VerificationKey2020",
    "controller": "did:p2p:device123",
    "publicKeyMultibase": "z6MkhaXgBZDvotDkL5257faiztiGiC2QtKLGpbnnEGta2doK"
  }],
  "authentication": ["did:p2p:device123#key-1"],
  "service": [{
    "id": "did:p2p:device123#p2p",
    "type": "P2PService",
    "serviceEndpoint": "p2p://example.com/device123"
  }]
}
```

### Sample Device Info
```json
{
  "device_id": "device123",
  "device_type": "mobile",
  "manufacturer": "Apple",
  "model": "iPhone 15",
  "os_version": "iOS 17.0",
  "capabilities": ["video", "audio", "screen_share"],
  "metadata": {
    "app_version": "1.0.0",
    "sdk_version": "2.0.0"
  }
}
```

## 🔧 Mock Strategy

### Redis Mock
- Mock all Redis operations (SET, GET, DEL, EXPIRE, HSET, HGET, etc.)
- Simulate connection failures
- Simulate timeout scenarios

### HTTP Mock
- Mock HTTP requests for API endpoints
- Simulate network errors
- Simulate timeout scenarios

## 📈 Coverage Measurement

### Tools
- `gcov` - Code coverage tool
- `lcov` - Coverage report generator
- `genhtml` - HTML report generator

### Commands
```bash
# Build with coverage
cmake -DENABLE_COVERAGE=ON ..
make

# Run tests
make test

# Generate coverage report
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
```

## ⏰ Timeline

- **Now - 4:00 AM**: Preparation phase
  - Study Python tests ✅
  - Design test structure ✅
  - Prepare mock framework
  - Set up test fixtures

- **4:00 AM - 6:00 AM**: Core tests (Priority 1)
  - test_crypto.cpp
  - test_storage.cpp
  - test_auth.cpp

- **6:00 AM - 8:00 AM**: Additional tests (Priority 2)
  - test_rate_limiter.cpp
  - test_validators.cpp

- **8:00 AM - 10:00 AM**: Coverage and polish
  - Run coverage analysis
  - Fix gaps
  - Documentation
  - Final verification

## ✅ Completion Criteria

- [ ] All 5 test modules implemented
- [ ] All tests pass
- [ ] Coverage ≥ 80% overall
- [ ] Core modules ≥ 90% coverage
- [ ] Mock framework complete
- [ ] Test documentation clear
- [ ] CMakeLists.txt configured
- [ ] Coverage report generated

## 📝 Notes

- Wait for engineer-client to complete #27 (DID Core) and #24 (DID API)
- Use existing STUN test structure as template
- Focus on critical paths first
- Ensure thread safety in tests
- Use RAII for resource management

## 🚀 Ready to Start

Status: 🟡 **Waiting for #27 and #24**

Once dependencies are complete, I will immediately begin implementation following this plan.
