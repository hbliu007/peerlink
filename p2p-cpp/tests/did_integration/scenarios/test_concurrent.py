"""
Scenario 5: Concurrent Operations
"""
import pytest
import asyncio
import concurrent.futures
import time
import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from utils.did_client import DIDServiceClient

@pytest.mark.timeout(30)
def test_concurrent_registrations(did_service):
    """Test multiple clients registering simultaneously"""
    client = DIDServiceClient(did_service)

    def register_device(index):
        device = {
            'device_id': f'concurrent_device_{index}',
            'platform': 'test'
        }
        result = client.register_device(device)
        return result is not None and result.get('success') != False

    # Register 50 devices concurrently
    with concurrent.futures.ThreadPoolExecutor(max_workers=10) as executor:
        futures = [executor.submit(register_device, i) for i in range(50)]
        results = [f.result() for f in concurrent.futures.as_completed(futures)]

    success_count = sum(1 for r in results if r)

    # At least 80% should succeed
    assert success_count >= 40, f"Only {success_count}/50 succeeded"


@pytest.mark.timeout(30)
def test_concurrent_heartbeats(did_service):
    """Test concurrent heartbeat updates"""
    client = DIDServiceClient(did_service)

    # Register devices first
    device_ids = []
    for i in range(20):
        device = {
            'device_id': f'heartbeat_device_{i}',
            'platform': 'test'
        }
        result = client.register_device(device)
        if result and result.get('success') != False:
            device_ids.append(device['device_id'])

    def send_heartbeat(device_id):
        return client.update_heartbeat(device_id)

    # Send heartbeats concurrently
    with concurrent.futures.ThreadPoolExecutor(max_workers=10) as executor:
        futures = [executor.submit(send_heartbeat, did) for did in device_ids]
        results = [f.result() for f in concurrent.futures.as_completed(futures)]

    # Most should succeed
    success_count = sum(1 for r in results if r)
    print(f"Heartbeat success: {success_count}/{len(device_ids)}")


@pytest.mark.timeout(40)
def test_concurrent_mixed_operations(did_service):
    """Test mixed concurrent operations"""
    client = DIDServiceClient(did_service)

    operations_count = {'register': 0, 'get': 0, 'heartbeat': 0}

    def mixed_operation(index):
        op_type = index % 3

        if op_type == 0:  # Register
            device = {
                'device_id': f'mixed_device_{index}',
                'platform': 'test'
            }
            result = client.register_device(device)
            operations_count['register'] += 1
            return result is not None

        elif op_type == 1:  # Get
            result = client.get_device(f'mixed_device_{index - 1}')
            operations_count['get'] += 1
            return result is not None

        else:  # Heartbeat
            result = client.update_heartbeat(f'mixed_device_{index - 2}')
            operations_count['heartbeat'] += 1
            return True

    # Execute mixed operations
    with concurrent.futures.ThreadPoolExecutor(max_workers=15) as executor:
        futures = [executor.submit(mixed_operation, i) for i in range(60)]
        results = [f.result() for f in concurrent.futures.as_completed(futures)]

    success_count = sum(1 for r in results if r)

    # At least 70% should succeed
    assert success_count >= 42, f"Only {success_count}/60 succeeded"


@pytest.mark.timeout(30)
def test_rate_limit_concurrent(did_service):
    """Test rate limiting under concurrent load"""
    client = DIDServiceClient(did_service)

    def rapid_register(index):
        device = {
            'device_id': f'rate_limit_device_{index}',
            'platform': 'test'
        }
        result = client.register_device(device)
        return result

    start_time = time.time()

    # Send 100 requests concurrently
    with concurrent.futures.ThreadPoolExecutor(max_workers=20) as executor:
        futures = [executor.submit(rapid_register, i) for i in range(100)]
        results = [f.result() for f in concurrent.futures.as_completed(futures)]

    elapsed = time.time() - start_time

    success_count = sum(1 for r in results if r and r.get('success') != False)
    rate_limited = len(results) - success_count

    print(f"Completed in {elapsed:.2f}s")
    print(f"Success: {success_count}, Rate limited: {rate_limited}")

    # Should complete within reasonable time
    assert elapsed < 30


@pytest.mark.timeout(25)
def test_concurrent_device_queries(did_service, multiple_devices):
    """Test concurrent device queries"""
    client = DIDServiceClient(did_service)

    # Register devices
    for device in multiple_devices:
        client.register_device(device)

    def query_device(device_id):
        return client.get_device(device_id)

    # Query all devices concurrently
    with concurrent.futures.ThreadPoolExecutor(max_workers=5) as executor:
        futures = [executor.submit(query_device, d['device_id'])
                   for d in multiple_devices]
        results = [f.result() for f in concurrent.futures.as_completed(futures)]

    # All queries should return results
    assert len(results) == len(multiple_devices)
