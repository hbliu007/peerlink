#pragma once

#include "p2p/crypto/ed25519_signer.hpp"
#include <vector>
#include <string>
#include <cstdint>
#include <optional>

namespace p2p {
namespace crypto {

/**
 * @brief Signed Envelope as per libp2p RFC 0002
 *
 * A signed envelope contains:
 * - public_key: The public key that signed the envelope
 * - payload_type: The type of payload (e.g., "/libp2p/relay-reservation")
 * - payload: The actual payload data
 * - signature: Ed25519 signature over the envelope
 *
 * Signature is computed over: "libp2p-signed-envelope:" + payload_type + payload
 */
struct SignedEnvelope {
    static constexpr const char* DOMAIN_STRING = "libp2p-signed-envelope:";

    std::vector<uint8_t> public_key;      // Ed25519 public key (32 bytes)
    std::string payload_type;             // Payload type identifier
    std::vector<uint8_t> payload;         // Payload data
    std::vector<uint8_t> signature;       // Ed25519 signature (64 bytes)

    SignedEnvelope() = default;

    /**
     * @brief Sign a payload and create a signed envelope
     *
     * @param private_key The Ed25519 private key to sign with
     * @param payload_type The type of payload (e.g., "/libp2p/relay-reservation")
     * @param payload The payload data to sign
     * @return SignedEnvelope The signed envelope
     */
    static SignedEnvelope Sign(
        const Ed25519PrivateKey& private_key,
        const std::string& payload_type,
        const std::vector<uint8_t>& payload
    );

    /**
     * @brief Verify the signature of a signed envelope
     *
     * @return true if signature is valid
     * @return false if signature is invalid
     */
    bool Verify() const;

    /**
     * @brief Verify the signature and check the payload type
     *
     * @param expected_payload_type The expected payload type
     * @return true if signature is valid and payload type matches
     * @return false otherwise
     */
    bool VerifyWithType(const std::string& expected_payload_type) const;

    /**
     * @brief Serialize the signed envelope to bytes
     *
     * Format: [public_key_len][public_key][payload_type_len][payload_type]
     *         [payload_len][payload][signature_len][signature]
     *
     * @return std::vector<uint8_t> The serialized envelope
     */
    std::vector<uint8_t> Serialize() const;

    /**
     * @brief Deserialize a signed envelope from bytes
     *
     * @param data The serialized envelope data
     * @return std::optional<SignedEnvelope> The envelope if successful, nullopt otherwise
     */
    static std::optional<SignedEnvelope> Deserialize(const std::vector<uint8_t>& data);

private:
    /**
     * @brief Compute the data to sign/verify
     *
     * @return std::vector<uint8_t> The data: domain_string + payload_type + payload
     */
    std::vector<uint8_t> ComputeSigningData() const;
};

} // namespace crypto
} // namespace p2p
