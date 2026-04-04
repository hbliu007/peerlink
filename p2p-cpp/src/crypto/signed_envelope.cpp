#include "p2p/crypto/signed_envelope.hpp"
#include <cstring>
#include <stdexcept>
#include <boost/endian/conversion.hpp>

namespace p2p {
namespace crypto {

SignedEnvelope SignedEnvelope::Sign(
    const Ed25519PrivateKey& private_key,
    const std::string& payload_type,
    const std::vector<uint8_t>& payload
) {
    SignedEnvelope envelope;

    // Derive public key from private key
    Ed25519PublicKey public_key = Ed25519Signer::DerivePublicKey(private_key);
    envelope.public_key = public_key.GetKeyData();
    envelope.payload_type = payload_type;
    envelope.payload = payload;

    // Compute signing data: domain_string + payload_type + payload
    std::vector<uint8_t> signing_data = envelope.ComputeSigningData();

    // Sign the data
    Ed25519Signature signature = Ed25519Signer::Sign(private_key, signing_data);
    envelope.signature = signature.data;

    return envelope;
}

bool SignedEnvelope::Verify() const {
    if (public_key.size() != Ed25519PublicKey::KEY_SIZE) {
        return false;
    }

    if (signature.size() != Ed25519Signature::SIGNATURE_SIZE) {
        return false;
    }

    // Compute signing data
    std::vector<uint8_t> signing_data = ComputeSigningData();

    // Verify signature
    Ed25519PublicKey pub_key(public_key);
    Ed25519Signature sig;
    sig.data = signature;

    return Ed25519Signer::Verify(pub_key, signing_data, sig);
}

bool SignedEnvelope::VerifyWithType(const std::string& expected_payload_type) const {
    if (payload_type != expected_payload_type) {
        return false;
    }

    return Verify();
}

std::vector<uint8_t> SignedEnvelope::Serialize() const {
    std::vector<uint8_t> result;

    // Serialize public_key with explicit big-endian byte order
    uint32_t public_key_len = static_cast<uint32_t>(public_key.size());
    uint32_t public_key_len_be = boost::endian::native_to_big(public_key_len);
    result.insert(result.end(),
                  reinterpret_cast<const uint8_t*>(&public_key_len_be),
                  reinterpret_cast<const uint8_t*>(&public_key_len_be) + sizeof(public_key_len_be));
    result.insert(result.end(), public_key.begin(), public_key.end());

    // Serialize payload_type with explicit big-endian byte order
    uint32_t payload_type_len = static_cast<uint32_t>(payload_type.size());
    uint32_t payload_type_len_be = boost::endian::native_to_big(payload_type_len);
    result.insert(result.end(),
                  reinterpret_cast<const uint8_t*>(&payload_type_len_be),
                  reinterpret_cast<const uint8_t*>(&payload_type_len_be) + sizeof(payload_type_len_be));
    result.insert(result.end(), payload_type.begin(), payload_type.end());

    // Serialize payload with explicit big-endian byte order
    uint32_t payload_len = static_cast<uint32_t>(payload.size());
    uint32_t payload_len_be = boost::endian::native_to_big(payload_len);
    result.insert(result.end(),
                  reinterpret_cast<const uint8_t*>(&payload_len_be),
                  reinterpret_cast<const uint8_t*>(&payload_len_be) + sizeof(payload_len_be));
    result.insert(result.end(), payload.begin(), payload.end());

    // Serialize signature with explicit big-endian byte order
    uint32_t signature_len = static_cast<uint32_t>(signature.size());
    uint32_t signature_len_be = boost::endian::native_to_big(signature_len);
    result.insert(result.end(),
                  reinterpret_cast<const uint8_t*>(&signature_len_be),
                  reinterpret_cast<const uint8_t*>(&signature_len_be) + sizeof(signature_len_be));
    result.insert(result.end(), signature.begin(), signature.end());

    return result;
}

std::optional<SignedEnvelope> SignedEnvelope::Deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < sizeof(uint32_t) * 4) {
        return std::nullopt;
    }

    SignedEnvelope envelope;
    size_t offset = 0;

    // Deserialize public_key with explicit big-endian conversion
    uint32_t public_key_len_be;
    std::memcpy(&public_key_len_be, data.data() + offset, sizeof(public_key_len_be));
    uint32_t public_key_len = boost::endian::big_to_native(public_key_len_be);
    offset += sizeof(public_key_len_be);

    if (offset + public_key_len > data.size()) {
        return std::nullopt;
    }

    envelope.public_key.assign(data.begin() + offset, data.begin() + offset + public_key_len);
    offset += public_key_len;

    // Deserialize payload_type with explicit big-endian conversion
    uint32_t payload_type_len_be;
    std::memcpy(&payload_type_len_be, data.data() + offset, sizeof(payload_type_len_be));
    uint32_t payload_type_len = boost::endian::big_to_native(payload_type_len_be);
    offset += sizeof(payload_type_len_be);

    if (offset + payload_type_len > data.size()) {
        return std::nullopt;
    }

    envelope.payload_type.assign(data.begin() + offset, data.begin() + offset + payload_type_len);
    offset += payload_type_len;

    // Deserialize payload with explicit big-endian conversion
    uint32_t payload_len_be;
    std::memcpy(&payload_len_be, data.data() + offset, sizeof(payload_len_be));
    uint32_t payload_len = boost::endian::big_to_native(payload_len_be);
    offset += sizeof(payload_len_be);

    if (offset + payload_len > data.size()) {
        return std::nullopt;
    }

    envelope.payload.assign(data.begin() + offset, data.begin() + offset + payload_len);
    offset += payload_len;

    // Deserialize signature with explicit big-endian conversion
    uint32_t signature_len_be;
    std::memcpy(&signature_len_be, data.data() + offset, sizeof(signature_len_be));
    uint32_t signature_len = boost::endian::big_to_native(signature_len_be);
    offset += sizeof(signature_len_be);

    if (offset + signature_len > data.size()) {
        return std::nullopt;
    }

    envelope.signature.assign(data.begin() + offset, data.begin() + offset + signature_len);
    offset += signature_len;

    return envelope;
}

std::vector<uint8_t> SignedEnvelope::ComputeSigningData() const {
    std::vector<uint8_t> signing_data;

    // Add domain string
    const char* domain = DOMAIN_STRING;
    size_t domain_len = std::strlen(domain);
    signing_data.insert(signing_data.end(), domain, domain + domain_len);

    // Add payload_type
    signing_data.insert(signing_data.end(), payload_type.begin(), payload_type.end());

    // Add payload
    signing_data.insert(signing_data.end(), payload.begin(), payload.end());

    return signing_data;
}

} // namespace crypto
} // namespace p2p
