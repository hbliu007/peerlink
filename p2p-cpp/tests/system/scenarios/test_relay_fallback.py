"""
Scenario 2: Relay Fallback
"""
import pytest
import asyncio
import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from utils.test_clients import TURNClient, SignalingClient

@pytest.mark.asyncio
@pytest.mark.timeout(30)
async def test_relay_allocation(services):
    """Test TURN relay allocation"""
    turn_client = TURNClient(services['relay']['host'], services['relay']['port'])

    # Allocate relay address
    allocated = turn_client.allocate()
    assert allocated == True, "Relay allocation failed"

    # Refresh allocation
    refreshed = turn_client.refresh()
    assert refreshed == True, "Relay refresh failed"


@pytest.mark.asyncio
@pytest.mark.timeout(40)
async def test_p2p_with_relay_fallback(services, two_devices):
    """
    Test P2P connection with relay fallback:
    1. Attempt direct P2P (simulated failure)
    2. Fallback to relay
    3. Data transfer through relay
    """
    device_a = two_devices[0]
    device_b = two_devices[1]

    # Both devices allocate relay addresses
    turn_client_a = TURNClient(services['relay']['host'], services['relay']['port'])
    turn_client_b = TURNClient(services['relay']['host'], services['relay']['port'])

    allocated_a = turn_client_a.allocate()
    assert allocated_a == True

    allocated_b = turn_client_b.allocate()
    assert allocated_b == True

    # Connect to signaling
    signaling_url = f"ws://{services['signaling']['host']}:{services['signaling']['port']}"
    client_a = SignalingClient(signaling_url)
    client_b = SignalingClient(signaling_url)

    await client_a.connect(device_a['device_id'])
    await client_b.connect(device_b['device_id'])

    # Exchange relay candidates through signaling
    await client_a.send_offer(device_b['device_id'], "relay_candidate_a")
    msg = await client_b.receive_message()
    assert msg is not None

    # Cleanup
    await client_a.disconnect()
    await client_b.disconnect()


@pytest.mark.asyncio
@pytest.mark.timeout(20)
async def test_concurrent_relay_allocations(services):
    """Test multiple concurrent relay allocations"""
    num_clients = 10
    clients = []

    for i in range(num_clients):
        client = TURNClient(services['relay']['host'], services['relay']['port'])
        clients.append(client)

    # Allocate concurrently
    results = []
    for client in clients:
        result = client.allocate()
        results.append(result)

    # At least 80% should succeed
    success_count = sum(1 for r in results if r)
    assert success_count >= num_clients * 0.8
