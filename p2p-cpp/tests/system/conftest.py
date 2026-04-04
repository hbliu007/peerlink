"""
System test configuration and fixtures
"""
import pytest
import time
import os

# Service endpoints
SERVICES = {
    'stun': {
        'host': os.getenv('PEERLINK_TEST_STUN_HOST', 'localhost'),
        'port': int(os.getenv('PEERLINK_TEST_STUN_PORT', '3478')),
        'protocol': 'udp'
    },
    'signaling': {
        'host': os.getenv('PEERLINK_TEST_SIGNALING_HOST', 'localhost'),
        'port': int(os.getenv('PEERLINK_TEST_SIGNALING_PORT', '8080')),
        'protocol': 'ws'
    },
    'relay': {
        'host': os.getenv('PEERLINK_TEST_RELAY_HOST', 'localhost'),
        'port': int(os.getenv('PEERLINK_TEST_RELAY_PORT', '9001')),
        'protocol': 'udp'
    },
    'did': {
        'host': os.getenv('PEERLINK_TEST_DID_HOST', 'localhost'),
        'port': int(os.getenv('PEERLINK_TEST_DID_PORT', '8081')),
        'protocol': 'http'
    },
    'redis': {
        'host': os.getenv('PEERLINK_TEST_REDIS_HOST', 'localhost'),
        'port': int(os.getenv('PEERLINK_TEST_REDIS_PORT', '6379'))
    }
}

@pytest.fixture(scope='session')
def services():
    """Use externally managed native services."""
    yield SERVICES


@pytest.fixture
def test_device():
    """Create a test device"""
    return {
        'device_id': f'test_device_{int(time.time())}',
        'platform': 'test',
        'version': '1.0.0'
    }


@pytest.fixture
def two_devices():
    """Create two test devices"""
    timestamp = int(time.time())
    return [
        {
            'device_id': f'device_a_{timestamp}',
            'platform': 'ios',
            'version': '1.0.0'
        },
        {
            'device_id': f'device_b_{timestamp}',
            'platform': 'android',
            'version': '1.0.0'
        }
    ]
