"""
DID Integration Test Configuration
"""
import pytest
import time
import requests
import redis
import os

DID_SERVICE_URL = os.getenv(
    "PEERLINK_TEST_DID_SERVICE_URL",
    "http://localhost:8081",
)
REDIS_HOST = os.getenv("PEERLINK_TEST_REDIS_HOST", "localhost")
REDIS_PORT = int(os.getenv("PEERLINK_TEST_REDIS_PORT", "6379"))


@pytest.fixture(scope='session')
def did_service():
    """Wait for DID service to be ready"""
    max_wait = 30
    start_time = time.time()

    while time.time() - start_time < max_wait:
        try:
            resp = requests.get(f'{DID_SERVICE_URL}/healthz', timeout=2)
            if resp.status_code == 200:
                return DID_SERVICE_URL
        except requests.RequestException:
            pass
        time.sleep(1)

    raise RuntimeError("DID service failed to start")

@pytest.fixture(scope='session')
def redis_client():
    """Redis client for verification"""
    client = redis.Redis(host=REDIS_HOST, port=REDIS_PORT, decode_responses=True)

    # Wait for Redis
    max_wait = 10
    start_time = time.time()
    while time.time() - start_time < max_wait:
        try:
            client.ping()
            return client
        except redis.RedisError:
            time.sleep(0.5)

    raise RuntimeError("Redis failed to start")


@pytest.fixture
def test_device():
    """Generate test device data"""
    timestamp = int(time.time() * 1000)
    return {
        'device_id': f'test_device_{timestamp}',
        'platform': 'ios',
        'os_version': '17.0',
        'app_version': '1.0.0'
    }


@pytest.fixture
def multiple_devices():
    """Generate multiple test devices"""
    timestamp = int(time.time() * 1000)
    platforms = ['ios', 'android', 'web', 'desktop', 'mobile']

    devices = []
    for i, platform in enumerate(platforms):
        devices.append({
            'device_id': f'{platform}_device_{timestamp}_{i}',
            'platform': platform,
            'os_version': '1.0',
            'app_version': '1.0.0'
        })

    return devices


@pytest.fixture(autouse=True)
def cleanup_redis(redis_client):
    """Cleanup Redis after each test"""
    yield
    # Cleanup test keys
    for key in redis_client.scan_iter("device:*"):
        redis_client.delete(key)
    for key in redis_client.scan_iter("did:*"):
        redis_client.delete(key)
