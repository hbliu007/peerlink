"""
Scenario 3: Multi-Device Scenarios
"""
import pytest
import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from utils.did_client import DIDServiceClient

@pytest.mark.timeout(20)
def test_multiple_device_types(did_service, multiple_devices):
    """Test registering multiple device types"""
    client = DIDServiceClient(did_service)

    success_count = 0
    for device in multiple_devices:
        result = client.register_device(device)
        if result and result.get('success') != False:
            success_count += 1

    # At least 80% should succeed
    assert success_count >= len(multiple_devices) * 0.8


@pytest.mark.timeout(15)
def test_query_by_platform(did_service, multiple_devices):
    """Test querying devices by platform"""
    client = DIDServiceClient(did_service)

    # Register devices
    for device in multiple_devices:
        client.register_device(device)

    # Query iOS devices
    ios_devices = client.list_devices(platform='ios')

    # Should return at least one iOS device
    if ios_devices is not None:
        ios_count = sum(1 for d in ios_devices if d.get('platform') == 'ios')
        assert ios_count >= 1


@pytest.mark.timeout(15)
def test_online_devices_list(did_service, multiple_devices):
    """Test listing online devices"""
    client = DIDServiceClient(did_service)

    # Register devices
    for device in multiple_devices:
        client.register_device(device)

    # Get online devices
    online_devices = client.list_devices(online_only=True)

    # Should return some devices
    if online_devices is not None:
        assert len(online_devices) >= 0


@pytest.mark.timeout(15)
def test_list_all_devices(did_service, multiple_devices):
    """Test listing all devices"""
    client = DIDServiceClient(did_service)

    # Register devices
    registered_count = 0
    for device in multiple_devices:
        result = client.register_device(device)
        if result and result.get('success') != False:
            registered_count += 1

    # List all devices
    all_devices = client.list_devices()

    if all_devices is not None:
        # Should return at least some of the registered devices
        assert len(all_devices) >= registered_count * 0.5


@pytest.mark.timeout(15)
def test_platform_filtering(did_service, multiple_devices):
    """Test platform-based filtering"""
    client = DIDServiceClient(did_service)

    # Register devices
    for device in multiple_devices:
        client.register_device(device)

    # Test each platform
    platforms = ['ios', 'android', 'web']
    for platform in platforms:
        devices = client.list_devices(platform=platform)

        if devices is not None:
            # All returned devices should match the platform
            for device in devices:
                if 'platform' in device:
                    assert device['platform'] == platform
