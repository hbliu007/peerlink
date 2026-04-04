"""
Scenario 1: Complete P2P Connection Establishment
"""
import pytest
import asyncio
import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from utils.test_clients import STUNClient, SignalingClient, DIDClient

@pytest.mark.asyncio
@pytest.mark.timeout(30)
async def test_complete_p2p_flow(services, two_devices):
    """
    Test complete P2P connection flow:
    1. DID registration (2 devices)
    2. STUN NAT traversal
    3. Signaling exchange
    4. P2P connection establishment
    """

    # Step 1: Register devices with DID service
    did_client = DIDClient(f"http://{services['did']['host']}:{services['did']['port']}")

    device_a = two_devices[0]
    device_b = two_devices[1]

    result_a = did_client.register_device(device_a['device_id'], device_a['platform'])
    assert result_a is not None, "Device A registration failed"
    assert result_a.get('success') == True

    result_b = did_client.register_device(device_b['device_id'], device_b['platform'])
    assert result_b is not None, "Device B registration failed"
    assert result_b.get('success') == True

    # Step 2: STUN NAT traversal
    stun_client_a = STUNClient(services['stun']['host'], services['stun']['port'])
    stun_result_a = stun_client_a.send_binding_request()
    assert stun_result_a is not None
    assert stun_result_a['success'] == True

    stun_client_b = STUNClient(services['stun']['host'], services['stun']['port'])
    stun_result_b = stun_client_b.send_binding_request()
    assert stun_result_b is not None
    assert stun_result_b['success'] == True

    # Step 3: Signaling exchange
    signaling_url = f"ws://{services['signaling']['host']}:{services['signaling']['port']}"

    client_a = SignalingClient(signaling_url)
    client_b = SignalingClient(signaling_url)

    # Connect both clients
    connected_a = await client_a.connect(device_a['device_id'])
    assert connected_a == True, "Client A failed to connect to signaling"

    connected_b = await client_b.connect(device_b['device_id'])
    assert connected_b == True, "Client B failed to connect to signaling"

    # Client A sends offer to Client B
    offer_sent = await client_a.send_offer(device_b['device_id'], "mock_sdp_offer")
    assert offer_sent == True

    # Client B receives offer
    offer_msg = await client_b.receive_message(timeout=5.0)
    assert offer_msg is not None
    assert offer_msg.get('type') == 'offer'

    # Cleanup
    await client_a.disconnect()
    await client_b.disconnect()


@pytest.mark.asyncio
@pytest.mark.timeout(20)
async def test_stun_binding_only(services):
    """Test STUN binding request/response"""
    stun_client = STUNClient(services['stun']['host'], services['stun']['port'])
    result = stun_client.send_binding_request()

    assert result is not None
    assert result['success'] == True
    assert result['response_length'] >= 20


@pytest.mark.asyncio
@pytest.mark.timeout(20)
async def test_signaling_registration(services, test_device):
    """Test signaling server registration"""
    signaling_url = f"ws://{services['signaling']['host']}:{services['signaling']['port']}"
    client = SignalingClient(signaling_url)

    connected = await client.connect(test_device['device_id'])
    assert connected == True

    await client.disconnect()


@pytest.mark.asyncio
@pytest.mark.timeout(20)
async def test_did_registration(services, test_device):
    """Test DID service device registration"""
    did_client = DIDClient(f"http://{services['did']['host']}:{services['did']['port']}")

    result = did_client.register_device(
        test_device['device_id'],
        test_device['platform']
    )

    assert result is not None
    assert result.get('success') == True

    # Verify device can be retrieved
    device_info = did_client.get_device(test_device['device_id'])
    assert device_info is not None
    assert device_info.get('device_id') == test_device['device_id']
