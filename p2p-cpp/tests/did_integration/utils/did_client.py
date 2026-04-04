"""
DID Service Client for Integration Testing
"""
import requests
import hashlib
from typing import Optional, Dict, List


class DIDServiceClient:
    """Client for DID service API"""

    def __init__(self, base_url: str):
        self.base_url = base_url
        self.session = requests.Session()

    def register_device(self, device_data: Dict[str, str]) -> Optional[Dict]:
        """Register a new device"""
        try:
            response = self.session.post(
                f'{self.base_url}/api/v1/devices/register',
                json=device_data,
                timeout=5.0
            )

            if response.status_code in [200, 201]:
                return response.json()
            else:
                return {
                    'success': False,
                    'status_code': response.status_code,
                    'error': response.text
                }

        except Exception as e:
            return {'success': False, 'error': str(e)}

    def get_device(self, device_id: str) -> Optional[Dict]:
        """Get device information"""
        try:
            response = self.session.get(
                f'{self.base_url}/api/v1/devices/{device_id}',
                timeout=5.0
            )

            if response.status_code == 200:
                return response.json()
            elif response.status_code == 404:
                return {'success': False, 'error': 'not_found'}
            else:
                return {'success': False, 'status_code': response.status_code}

        except Exception as e:
            return {'success': False, 'error': str(e)}

    def update_heartbeat(self, device_id: str) -> bool:
        """Update device heartbeat"""
        try:
            response = self.session.post(
                f'{self.base_url}/api/v1/devices/{device_id}/heartbeat',
                timeout=5.0
            )

            return response.status_code == 200

        except requests.RequestException:
            return False

    def delete_device(self, device_id: str) -> bool:
        """Delete a device"""
        try:
            response = self.session.delete(
                f'{self.base_url}/api/v1/devices/{device_id}',
                timeout=5.0
            )

            return response.status_code in [200, 204]

        except requests.RequestException:
            return False

    def list_devices(self, platform: Optional[str] = None,
                     online_only: bool = False) -> Optional[List[Dict]]:
        """List devices with optional filters"""
        try:
            params = {}
            if platform:
                params['platform'] = platform
            if online_only:
                params['online'] = 'true'

            response = self.session.get(
                f'{self.base_url}/api/v1/devices',
                params=params,
                timeout=5.0
            )

            if response.status_code == 200:
                return response.json().get('devices', [])

        except requests.RequestException:
            pass

        return None

    def verify_did(self, did: str, signature: str) -> bool:
        """Verify DID signature"""
        try:
            response = self.session.post(
                f'{self.base_url}/api/v1/did/verify',
                json={'did': did, 'signature': signature},
                timeout=5.0
            )

            return (
                response.status_code == 200 and
                response.json().get('valid') is True
            )

        except requests.RequestException:
            return False

    def get_token(self, device_id: str) -> Optional[str]:
        """Get authentication token for device"""
        try:
            response = self.session.post(
                f'{self.base_url}/api/v1/auth/token',
                json={'device_id': device_id},
                timeout=5.0
            )

            if response.status_code == 200:
                return response.json().get('token')

        except requests.RequestException:
            pass

        return None

    def health_check(self) -> bool:
        """Check service health"""
        try:
            response = self.session.get(
                f'{self.base_url}/healthz',
                timeout=2.0
            )

            return response.status_code == 200

        except requests.RequestException:
            return False


def generate_mock_did(device_id: str) -> str:
    """Generate a mock DID for testing"""
    hash_obj = hashlib.sha256(device_id.encode())
    return f"did:p2p:{hash_obj.hexdigest()[:32]}"


def generate_mock_signature(did: str) -> str:
    """Generate a mock signature for testing"""
    hash_obj = hashlib.sha256(did.encode())
    return hash_obj.hexdigest()
