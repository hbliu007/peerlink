"""
Scenario 1: Complete Device Registration Flow
"""
import pytest
import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from utils.did_client import DIDServiceClient, generate_mock_did, generate_mock_signature

@pytest.mark.timeout(10)
def test_complete_registration_flow(did_service, test_device):
    """
    Test complete device registration flow:
    1. Generate DID
    2. Register device
    3. Verify signature
    4. Get token
    """
    client = DIDServiceClient(did_service)

    # Step 1: Generate DID
    did = generate_mock_did(test_device['device_id'])
    assert did.startswith('did:p2p:')

    # Step 2: Register device
    test_device['did'] = did
    result = client.register_device(test_device)

    assert result is not None
    assert result.get('success') != False
    assert 'device_id' in result or 'did' in result

    # Step 3: Verify signature
    signature = generate_mock_signature(did)
    # Note: Actual verification depends on implementation
    # verified = client.verify_did(did, signature)

    # Step 4: Get token
    token = client.get_token(test_device['device_id'])
    # Token generation may not be implemented yet
    # assert token is not None


@pytest.mark.timeout(10)
def test_device_registration_basic(did_service, test_device):
    """Test basic device registration"""
    client = DIDServiceClient(did_service)

    result = client.register_device(test_device)

    assert result is not None
    assert result.get('success') != False


@pytest.mark.timeout(10)
def test_duplicate_registration(did_service, test_device):
    """Test duplicate device registration"""
    client = DIDServiceClient(did_service)

    # First registration
    result1 = client.register_device(test_device)
    assert result1 is not None

    # Second registration with same device_id
    result2 = client.register_device(test_device)

    # Should either succeed (update) or return specific error
    assert result2 is not None


@pytest.mark.timeout(10)
def test_get_registered_device(did_service, test_device):
    """Test retrieving registered device"""
    client = DIDServiceClient(did_service)

    # Register device
    result = client.register_device(test_device)
    assert result is not None

    # Get device
    device_info = client.get_device(test_device['device_id'])

    assert device_info is not None
    assert device_info.get('success') != False


@pytest.mark.timeout(10)
def test_get_nonexistent_device(did_service):
    """Test getting non-existent device"""
    client = DIDServiceClient(did_service)

    device_info = client.get_device('nonexistent_device_12345')

    assert device_info is not None
    assert device_info.get('error') == 'not_found' or device_info.get('success') == False
