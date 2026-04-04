#include "servers/did/did_crypto.hpp"
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/sha.h>
#include <memory>
#include <sstream>
#include <vector>

namespace p2p {
namespace did {

namespace {

using EvpPkeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using EvpPkeyCtxPtr = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;
using EvpMdCtxPtr = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
using BioPtr = std::unique_ptr<BIO, decltype(&BIO_free)>;

std::string bytes_to_hex(const unsigned char* data, std::size_t size) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string encoded;
    encoded.reserve(size * 2);
    for (std::size_t i = 0; i < size; ++i) {
        encoded.push_back(kHex[(data[i] >> 4) & 0x0F]);
        encoded.push_back(kHex[data[i] & 0x0F]);
    }
    return encoded;
}

std::vector<unsigned char> hex_to_bytes(const std::string& hex) {
    if (hex.size() % 2 != 0) {
        return {};
    }

    auto decode_nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f') {
            return 10 + (c - 'a');
        }
        if (c >= 'A' && c <= 'F') {
            return 10 + (c - 'A');
        }
        return -1;
    };

    std::vector<unsigned char> bytes;
    bytes.reserve(hex.size() / 2);
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        const int high = decode_nibble(hex[i]);
        const int low = decode_nibble(hex[i + 1]);
        if (high < 0 || low < 0) {
            return {};
        }
        bytes.push_back(static_cast<unsigned char>((high << 4) | low));
    }
    return bytes;
}

std::string extract_pem_block(const std::string& input,
                              const std::string& begin_marker,
                              const std::string& end_marker) {
    const auto begin = input.find(begin_marker);
    if (begin == std::string::npos) {
        return {};
    }

    const auto end = input.find(end_marker, begin);
    if (end == std::string::npos) {
        return {};
    }

    const auto block_end = input.find('\n', end + end_marker.size());
    if (block_end == std::string::npos) {
        return input.substr(begin);
    }
    return input.substr(begin, block_end - begin + 1);
}

EvpPkeyPtr load_private_key(const std::string& private_key) {
    const std::string pem = extract_pem_block(
        private_key,
        "-----BEGIN PRIVATE KEY-----",
        "-----END PRIVATE KEY-----");
    if (pem.empty()) {
        return EvpPkeyPtr(nullptr, &EVP_PKEY_free);
    }

    BioPtr bio(BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size())), &BIO_free);
    if (!bio) {
        return EvpPkeyPtr(nullptr, &EVP_PKEY_free);
    }

    return EvpPkeyPtr(PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr), &EVP_PKEY_free);
}

EvpPkeyPtr load_public_key(const std::string& public_key) {
    const std::string pem = extract_pem_block(
        public_key,
        "-----BEGIN PUBLIC KEY-----",
        "-----END PUBLIC KEY-----");
    if (pem.empty()) {
        return EvpPkeyPtr(nullptr, &EVP_PKEY_free);
    }

    BioPtr bio(BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size())), &BIO_free);
    if (!bio) {
        return EvpPkeyPtr(nullptr, &EVP_PKEY_free);
    }

    return EvpPkeyPtr(PEM_read_bio_PUBKEY(bio.get(), nullptr, nullptr, nullptr), &EVP_PKEY_free);
}

std::string pem_string_from_bio(BIO* bio) {
    BUF_MEM* memory = nullptr;
    BIO_get_mem_ptr(bio, &memory);
    if (memory == nullptr || memory->data == nullptr || memory->length == 0) {
        return {};
    }
    return std::string(memory->data, memory->length);
}

}  // namespace

std::string DidCrypto::GenerateKeyPair() {
    EvpPkeyCtxPtr context(EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr), &EVP_PKEY_CTX_free);
    if (!context || EVP_PKEY_keygen_init(context.get()) <= 0) {
        return {};
    }

    EVP_PKEY* raw_key = nullptr;
    if (EVP_PKEY_keygen(context.get(), &raw_key) <= 0 || raw_key == nullptr) {
        return {};
    }

    EvpPkeyPtr key(raw_key, &EVP_PKEY_free);

    BioPtr private_bio(BIO_new(BIO_s_mem()), &BIO_free);
    BioPtr public_bio(BIO_new(BIO_s_mem()), &BIO_free);
    if (!private_bio || !public_bio) {
        return {};
    }

    if (PEM_write_bio_PrivateKey(private_bio.get(), key.get(), nullptr, nullptr, 0, nullptr, nullptr) <= 0) {
        return {};
    }
    if (PEM_write_bio_PUBKEY(public_bio.get(), key.get()) <= 0) {
        return {};
    }

    return pem_string_from_bio(private_bio.get()) + pem_string_from_bio(public_bio.get());
}

std::string DidCrypto::Sign(const std::string& data, const std::string& privateKey) {
    auto key = load_private_key(privateKey);
    if (!key) {
        return {};
    }

    EvpMdCtxPtr context(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
    if (!context || EVP_DigestSignInit(context.get(), nullptr, nullptr, nullptr, key.get()) <= 0) {
        return {};
    }

    std::size_t signature_size = 0;
    if (EVP_DigestSign(context.get(),
                       nullptr,
                       &signature_size,
                       reinterpret_cast<const unsigned char*>(data.data()),
                       data.size()) <= 0) {
        return {};
    }

    std::vector<unsigned char> signature(signature_size);
    if (EVP_DigestSign(context.get(),
                       signature.data(),
                       &signature_size,
                       reinterpret_cast<const unsigned char*>(data.data()),
                       data.size()) <= 0) {
        return {};
    }

    signature.resize(signature_size);
    return bytes_to_hex(signature.data(), signature.size());
}

bool DidCrypto::Verify(const std::string& data, const std::string& signature, const std::string& publicKey) {
    auto key = load_public_key(publicKey);
    auto signature_bytes = hex_to_bytes(signature);
    if (!key || signature_bytes.empty()) {
        return false;
    }

    EvpMdCtxPtr context(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
    if (!context || EVP_DigestVerifyInit(context.get(), nullptr, nullptr, nullptr, key.get()) <= 0) {
        return false;
    }

    return EVP_DigestVerify(context.get(),
                            signature_bytes.data(),
                            signature_bytes.size(),
                            reinterpret_cast<const unsigned char*>(data.data()),
                            data.size()) == 1;
}

std::string DidCrypto::Hash(const std::string& data) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    if (EVP_Q_digest(nullptr,
                     "SHA256",
                     nullptr,
                     reinterpret_cast<const unsigned char*>(data.data()),
                     data.size(),
                     digest,
                     nullptr) != 1) {
        return {};
    }
    return bytes_to_hex(digest, SHA256_DIGEST_LENGTH);
}

} // namespace did
} // namespace p2p
