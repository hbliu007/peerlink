#pragma once

#include <string>

namespace p2p {
namespace did {

class DidCrypto {
public:
    static std::string GenerateKeyPair();
    static std::string Sign(const std::string& data, const std::string& privateKey);
    static bool Verify(const std::string& data, const std::string& signature, const std::string& publicKey);
    static std::string Hash(const std::string& data);
};

using DIDCrypto = DidCrypto;

} // namespace did
} // namespace p2p
