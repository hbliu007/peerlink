#include "servers/did/did_auth.hpp"
#include <nlohmann/json.hpp>
#include <openssl/crypto.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <chrono>
#include <optional>
#include <vector>

namespace p2p {
namespace did {

namespace {

using json = nlohmann::json;

constexpr std::chrono::seconds kDefaultTokenLifetime{3600};

std::string base64_url_encode(const unsigned char* data, std::size_t length) {
    if (length == 0) {
        return {};
    }

    const auto encoded_size = 4 * ((length + 2) / 3);
    std::string encoded(encoded_size, '\0');
    const int actual_size = EVP_EncodeBlock(
        reinterpret_cast<unsigned char*>(encoded.data()),
        data,
        static_cast<int>(length));
    if (actual_size <= 0) {
        return {};
    }

    encoded.resize(static_cast<std::size_t>(actual_size));
    for (char& c : encoded) {
        if (c == '+') {
            c = '-';
        } else if (c == '/') {
            c = '_';
        }
    }
    while (!encoded.empty() && encoded.back() == '=') {
        encoded.pop_back();
    }
    return encoded;
}

std::vector<unsigned char> base64_url_decode(const std::string& input) {
    if (input.empty()) {
        return {};
    }

    std::string padded = input;
    for (char& c : padded) {
        if (c == '-') {
            c = '+';
        } else if (c == '_') {
            c = '/';
        }
    }
    while (padded.size() % 4 != 0) {
        padded.push_back('=');
    }

    std::vector<unsigned char> decoded((padded.size() / 4) * 3);
    const int decoded_size = EVP_DecodeBlock(
        decoded.data(),
        reinterpret_cast<const unsigned char*>(padded.data()),
        static_cast<int>(padded.size()));
    if (decoded_size < 0) {
        return {};
    }

    std::size_t padding = 0;
    if (!padded.empty() && padded.back() == '=') {
        ++padding;
    }
    if (padded.size() > 1 && padded[padded.size() - 2] == '=') {
        ++padding;
    }

    decoded.resize(static_cast<std::size_t>(decoded_size) - padding);
    return decoded;
}

std::vector<unsigned char> hmac_sha256(const std::string& secret, const std::string& payload) {
    unsigned int length = EVP_MAX_MD_SIZE;
    std::vector<unsigned char> digest(length);

    if (HMAC(EVP_sha256(),
             secret.data(),
             static_cast<int>(secret.size()),
             reinterpret_cast<const unsigned char*>(payload.data()),
             payload.size(),
             digest.data(),
             &length) == nullptr) {
        return {};
    }

    digest.resize(length);
    return digest;
}

std::vector<unsigned char> random_bytes(std::size_t size) {
    std::vector<unsigned char> bytes(size);
    if (RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1) {
        return {};
    }
    return bytes;
}

bool constant_time_equals(const std::vector<unsigned char>& left,
                          const std::vector<unsigned char>& right) {
    if (left.size() != right.size()) {
        return false;
    }
    return CRYPTO_memcmp(left.data(), right.data(), left.size()) == 0;
}

std::optional<json> parse_verified_payload(const std::string& token, const std::string& secret) {
    if (token.empty() || secret.empty()) {
        return std::nullopt;
    }

    const auto first_dot = token.find('.');
    const auto second_dot = token.find('.', first_dot == std::string::npos ? first_dot : first_dot + 1);
    if (first_dot == std::string::npos || second_dot == std::string::npos || token.find('.', second_dot + 1) != std::string::npos) {
        return std::nullopt;
    }

    const std::string header_part = token.substr(0, first_dot);
    const std::string payload_part = token.substr(first_dot + 1, second_dot - first_dot - 1);
    const std::string signature_part = token.substr(second_dot + 1);
    if (header_part.empty() || payload_part.empty() || signature_part.empty()) {
        return std::nullopt;
    }

    const auto header_bytes = base64_url_decode(header_part);
    const auto payload_bytes = base64_url_decode(payload_part);
    const auto signature_bytes = base64_url_decode(signature_part);
    if (header_bytes.empty() || payload_bytes.empty() || signature_bytes.empty()) {
        return std::nullopt;
    }

    const std::string signing_input = header_part + "." + payload_part;
    const auto expected_signature = hmac_sha256(secret, signing_input);
    if (expected_signature.empty() || !constant_time_equals(expected_signature, signature_bytes)) {
        return std::nullopt;
    }

    const auto header = json::parse(header_bytes.begin(), header_bytes.end(), nullptr, false);
    const auto payload = json::parse(payload_bytes.begin(), payload_bytes.end(), nullptr, false);
    if (header.is_discarded() || payload.is_discarded()) {
        return std::nullopt;
    }

    if (!header.is_object() ||
        header.value("alg", "") != "HS256" ||
        header.value("typ", "") != "JWT" ||
        !payload.is_object() ||
        !payload.contains("sub") ||
        !payload["sub"].is_string() ||
        !payload.contains("exp") ||
        !payload["exp"].is_number_integer()) {
        return std::nullopt;
    }

    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    if (payload["exp"].get<int64_t>() <= now) {
        return std::nullopt;
    }

    return payload;
}

}  // namespace

DidAuth::DidAuth(const std::string& jwt_secret)
    : jwt_secret_(jwt_secret) {
}

std::string DidAuth::GenerateToken(const std::string& did) {
    if (did.empty() || jwt_secret_.empty()) {
        return {};
    }

    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const auto nonce = random_bytes(8);
    if (nonce.empty()) {
        return {};
    }

    const json header = {
        {"alg", "HS256"},
        {"typ", "JWT"}
    };
    const json payload = {
        {"sub", did},
        {"iat", now},
        {"exp", now + kDefaultTokenLifetime.count()},
        {"nonce", base64_url_encode(nonce.data(), nonce.size())}
    };

    const std::string payload_dump = payload.dump();
    const std::string header_dump = header.dump();
    const std::string encoded_header = base64_url_encode(
        reinterpret_cast<const unsigned char*>(header_dump.data()),
        header_dump.size());
    const std::string encoded_payload = base64_url_encode(
        reinterpret_cast<const unsigned char*>(payload_dump.data()),
        payload_dump.size());
    if (encoded_header.empty() || encoded_payload.empty()) {
        return {};
    }

    const std::string signing_input = encoded_header + "." + encoded_payload;
    const auto signature = hmac_sha256(jwt_secret_, signing_input);
    if (signature.empty()) {
        return {};
    }

    return signing_input + "." + base64_url_encode(signature.data(), signature.size());
}

bool DidAuth::ValidateToken(const std::string& token) {
    return parse_verified_payload(token, jwt_secret_).has_value();
}

std::string DidAuth::ExtractDid(const std::string& token) {
    const auto payload = parse_verified_payload(token, jwt_secret_);
    if (!payload) {
        return {};
    }
    return payload->value("sub", "");
}

} // namespace did
} // namespace p2p
