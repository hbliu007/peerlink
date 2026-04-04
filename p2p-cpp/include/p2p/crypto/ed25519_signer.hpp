#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <memory>

namespace p2p {
namespace crypto {

/**
 * @brief Ed25519 private key (32 bytes)
 */
class Ed25519PrivateKey {
public:
    static constexpr size_t KEY_SIZE = 32;

    Ed25519PrivateKey() = default;
    explicit Ed25519PrivateKey(const std::vector<uint8_t>& key_data);
    explicit Ed25519PrivateKey(const uint8_t* key_data, size_t size);

    const std::vector<uint8_t>& GetKeyData() const { return key_data_; }
    std::vector<uint8_t> GetPublicKey() const;

private:
    std::vector<uint8_t> key_data_;
};

/**
 * @brief Ed25519 public key (32 bytes)
 */
class Ed25519PublicKey {
public:
    static constexpr size_t KEY_SIZE = 32;

    Ed25519PublicKey() = default;
    explicit Ed25519PublicKey(const std::vector<uint8_t>& key_data);
    explicit Ed25519PublicKey(const uint8_t* key_data, size_t size);

    const std::vector<uint8_t>& GetKeyData() const { return key_data_; }

private:
    std::vector<uint8_t> key_data_;
};

/**
 * @brief Ed25519 signature (64 bytes)
 */
struct Ed25519Signature {
    static constexpr size_t SIGNATURE_SIZE = 64;
    std::vector<uint8_t> data;

    Ed25519Signature() = default;
    explicit Ed25519Signature(const std::vector<uint8_t>& sig_data) : data(sig_data) {}
};

/**
 * @brief Ed25519 signer for signing and verifying data
 *
 * Uses OpenSSL EVP_PKEY_ED25519 for cryptographic operations.
 */
class Ed25519Signer {
public:
    Ed25519Signer() = default;
    ~Ed25519Signer() = default;

    /**
     * @brief Generate a new Ed25519 key pair
     *
     * @return Ed25519PrivateKey The generated private key
     */
    static Ed25519PrivateKey GenerateKeyPair();

    /**
     * @brief Sign data with Ed25519 private key
     *
     * @param private_key The private key to sign with
     * @param data The data to sign
     * @return Ed25519Signature The signature (64 bytes)
     */
    static Ed25519Signature Sign(
        const Ed25519PrivateKey& private_key,
        const std::vector<uint8_t>& data
    );

    /**
     * @brief Verify Ed25519 signature
     *
     * @param public_key The public key to verify with
     * @param data The original data
     * @param signature The signature to verify
     * @return true if signature is valid
     * @return false if signature is invalid
     */
    static bool Verify(
        const Ed25519PublicKey& public_key,
        const std::vector<uint8_t>& data,
        const Ed25519Signature& signature
    );

    /**
     * @brief Derive public key from private key
     *
     * @param private_key The private key
     * @return Ed25519PublicKey The corresponding public key
     */
    static Ed25519PublicKey DerivePublicKey(const Ed25519PrivateKey& private_key);
};

} // namespace crypto
} // namespace p2p
