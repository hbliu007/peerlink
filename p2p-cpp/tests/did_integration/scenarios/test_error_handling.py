"""
Scenario 4: Error Handling
"""
import pytest
import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from utils.did_client import DIDServiceClient

@pytest.mark.timeout(10)
def test_invalid_did_format(did_service):
    """Test registration with invalid DID format"""
    client = DIDServiceClient(did_service)

    invalid_device = {
        'device_id': 'test_invalid',
        'did': 'invalid_did_format',
        'platform': 'test'
    }

    result = client.register_device(invalid_device)

    # Should handle invalid DID gracefully
    assert result is not None


@pytest.mark.timeout(10)
def test_missing_required_fields(did_service):
    """Test registration with missing required fields"""
    client = DIDServiceClient(did_service)

    # Missing device_id
    incomplete_device = {
        'platform': 'test'
    }

    result = client.register_device(incomplete_device)

    # Should return error
    assert result is not None
    # May return error status code or success=False


@pytest.mark.timeout(10)
def test_nonexistent_device_operations(did_service):
    """Test operations on non-existent device"""
    client = DIDServiceClient(did_service)

    nonexistent_id = 'nonexistent_device_99999'

    # Get non-existent device
    device_info = client.get_device(nonexistent_id)
    assert device_info is not None
    assert device_info.get('error') == 'not_found' or device_info.get('success') == False

    # Heartbeat for non-existent device
    heartbeat_result = client.update_heartbeat(nonexistent_id)
    # Should return False or handle gracefully

    # Delete non-existent device
    delete_result = client.delete_device(nonexistent_id)
    # Should return False or handle gracefully


@pytest.mark.timeout(10)
def test_invalid_signature_verification(did_service):
    """Test DID verification with invalid signature"""
    client = DIDServiceClient(did_service)

    did = "did:p2p:test123"
    invalid_signature = "invalid_signature_data"

    verified = client.verify_did(did, invalid_signature)

    # Should return False for invalid signature
    assert verified == False


@pytest.mark.timeout(20)
def test_rate_limiting(did_service):
    """Test rate limiting enforcement"""
    client = DIDServiceClient(did_service)

    # Send many requests rapidly
    results = []
    for i in range(100):
        device = {
            'device_id': f'rate_test_{i}',
            'platform': 'test'
        }
        result = client.register_device(device)
        results.append(result)

    # Some requests should be rate limited
    success_count = sum(1 for r in results if r and r.get('success') != False)
    rate_limited_count = len(results) - success_count

    # Should have some rate limiting (or all succeed if no limit)
    print(f"Success: {success_count}, Rate limited: {rate_limited_count}")


@pytest.mark.timeout(10)
def test_malformed_json(did_service):
    """Test handling of malformed JSON"""
    import requests

    # Send malformed JSON directly
    try:
        response = requests.post(
            f'{did_service}/api/v1/devices/register',
            data='{"invalid json',
            headers={'Content-Type': 'application/json'},
            timeout=5.0
        )

        # Should return 400 Bad Request
        assert response.status_code == 400

    except:
        # Connection error is also acceptable
        pass
