"""
Scenario 4: Fault Recovery
"""
import pytest
import asyncio
import subprocess
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from utils.test_clients import SignalingClient, DIDClient


def run_restart_command(env_name):
    command = os.getenv(env_name, "").strip()
    if not command:
        pytest.skip(f"{env_name} is not set for restart testing")
    subprocess.run(command, shell=True, check=True)


@pytest.mark.asyncio
@pytest.mark.timeout(60)
async def test_device_reconnection(services, test_device):
    """Test device offline and reconnection"""
    signaling_url = (
        f"ws://{services['signaling']['host']}:{services['signaling']['port']}"
    )
    client = SignalingClient(signaling_url)

    # Initial connection
    connected = await client.connect(test_device['device_id'])
    assert connected

    # Disconnect
    await client.disconnect()
    await asyncio.sleep(2)

    # Reconnect
    connected = await client.connect(test_device['device_id'])
    assert connected

    await client.disconnect()


@pytest.mark.asyncio
@pytest.mark.timeout(90)
async def test_signaling_server_restart(services, test_device):
    """Test signaling server restart recovery"""
    signaling_url = (
        f"ws://{services['signaling']['host']}:{services['signaling']['port']}"
    )
    client = SignalingClient(signaling_url)

    # Connect before restart
    connected = await client.connect(test_device['device_id'])
    assert connected

    # Restart signaling server
    run_restart_command("PEERLINK_RESTART_SIGNALING_CMD")

    # Wait for server to restart
    await asyncio.sleep(10)

    # Try to reconnect
    await client.disconnect()
    client = SignalingClient(signaling_url)

    max_retries = 5
    for _ in range(max_retries):
        try:
            connected = await client.connect(test_device['device_id'])
            if connected:
                break
        except Exception:
            pass
        await asyncio.sleep(2)

    assert connected, "Failed to reconnect after server restart"
    await client.disconnect()


@pytest.mark.asyncio
@pytest.mark.timeout(60)
async def test_redis_connection_recovery(services, test_device):
    """Test Redis connection recovery"""
    did_client = DIDClient(
        f"http://{services['did']['host']}:{services['did']['port']}"
    )

    # Register device before Redis restart
    result = did_client.register_device(test_device['device_id'], test_device['platform'])
    assert result is not None

    # Restart Redis
    run_restart_command("PEERLINK_RESTART_REDIS_CMD")

    # Wait for Redis to restart
    await asyncio.sleep(5)

    # Try to register another device
    new_device_id = f"{test_device['device_id']}_after_restart"

    max_retries = 5
    success = False
    for _ in range(max_retries):
        result = did_client.register_device(
            new_device_id,
            test_device['platform'],
        )
        if result and result.get('success'):
            success = True
            break
        await asyncio.sleep(2)

    assert success, "Failed to recover after Redis restart"
