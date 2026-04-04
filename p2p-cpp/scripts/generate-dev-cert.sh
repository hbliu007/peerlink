#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CERT_DIR="${SCRIPT_DIR}/../certs"

mkdir -p "${CERT_DIR}"

echo "Generating self-signed certificate for development..."

# 生成私钥
openssl genrsa -out "${CERT_DIR}/server.key" 2048

# 生成证书签名请求（CSR）
openssl req -new -key "${CERT_DIR}/server.key" -out "${CERT_DIR}/server.csr" \
    -subj "/C=US/ST=California/L=San Francisco/O=PeerLink Dev/CN=localhost"

# 生成自签名证书（有效期 365 天）
openssl x509 -req -days 365 -in "${CERT_DIR}/server.csr" \
    -signkey "${CERT_DIR}/server.key" -out "${CERT_DIR}/server.crt" \
    -extfile <(printf "subjectAltName=DNS:localhost,IP:127.0.0.1")

# 清理 CSR
rm "${CERT_DIR}/server.csr"

echo "Certificate generated successfully:"
echo "  Certificate: ${CERT_DIR}/server.crt"
echo "  Private key: ${CERT_DIR}/server.key"
echo ""
echo "To use this certificate, add to your config:"
echo "  [signaling.tls]"
echo "  enabled = true"
echo "  cert_path = \"${CERT_DIR}/server.crt\""
echo "  key_path = \"${CERT_DIR}/server.key\""
