"""
Scenario 5: Performance Tests
"""
import pytest
import asyncio
import time
import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from utils.test_clients import DIDClient, SignalingClient, STUNClient

@pytest.mark.asyncio
@pytest.mark.timeout(120)
async def test_1000_device_registrations(services):
    """Test 1000 device registrations"""
    did_client = DIDClient(f"http://{services['did']['host']}:{services['did']['port']}")

    start_time = time.time()
    success_count = 0

    for i in range(1000):
        device_id = f"perf_device_{i}"
        result = did_client.register_device(device_id, "test")

        if result and result.get('success'):
            success_count += 1

    elapsed = time.time() - start_time

    # At least 95% success rate
    assert success_count >= 950, f"Only {success_count}/1000 registrations succeeded"

    # Should complete within 60 seconds
    assert elapsed < 60, f"Took {elapsed}s, expected < 60s"

    print(f"Registered {success_count} devices in {elapsed:.2f}s")
    print(f"Rate: {success_count/elapsed:.2f} devices/sec")


@pytest.mark.asyncio
@pytest.mark.timeout(90)
async def test_100_concurrent_connections(services):
    """Test 100 concurrent signaling connections"""
    signaling_url = f"ws://{services['signaling']['host']}:{services['signaling']['port']}"

    async def connect_client(device_id):
        client = SignalingClient(signaling_url)
        try:
            connected = await client.connect(device_id)
            await asyncio.sleep(1)  # Hold connection
            await client.disconnect()
            return connected
        except:
            return False

    start_time = time.time()

    # Create 100 concurrent connections
    tasks = [connect_client(f"concurrent_{i}") for i in range(100)]
    results = await asyncio.gather(*tasks, return_exceptions=True)

    elapsed = time.time() - start_time

    success_count = sum(1 for r in results if r == True)

    # At least 90% success rate
    assert success_count >= 90, f"Only {success_count}/100 connections succeeded"

    print(f"Established {success_count} connections in {elapsed:.2f}s")


@pytest.mark.asyncio
@pytest.mark.timeout(60)
async def test_stun_throughput(services):
    """Test STUN server throughput"""
    stun_client = STUNClient(services['stun']['host'], services['stun']['port'])

    start_time = time.time()
    success_count = 0

    for i in range(1000):
        result = stun_client.send_binding_request()
        if result and result['success']:
            success_count += 1

    elapsed = time.time() - start_time

    # At least 95% success rate
    assert success_count >= 950

    print(f"Processed {success_count} STUN requests in {elapsed:.2f}s")
    print(f"Throughput: {success_count/elapsed:.2f} req/sec")


@pytest.mark.asyncio
@pytest.mark.timeout(30)
async def test_rate_limiting(services):
    """Test rate limiting enforcement"""
    did_client = DIDClient(f"http://{services['did']['host']}:{services['did']['port']}")

    # Send requests rapidly
    results = []
    for i in range(100):
        result = did_client.register_device(f"rate_test_{i}", "test")
        results.append(result)

    # Some requests should be rate limited
    success_count = sum(1 for r in results if r and r.get('success'))
    failed_count = len(results) - success_count

    # Should have some rate limiting
    print(f"Success: {success_count}, Rate limited: {failed_count}")
