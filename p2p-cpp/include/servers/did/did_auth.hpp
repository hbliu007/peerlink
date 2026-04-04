#pragma once

#include <string>

namespace p2p {
namespace did {

class DidAuth {
public:
    explicit DidAuth(const std::string& jwt_secret);

    std::string GenerateToken(const std::string& did);
    bool ValidateToken(const std::string& token);
    std::string ExtractDid(const std::string& token);

private:
    std::string jwt_secret_;
};

using DIDAuth = DidAuth;

} // namespace did
} // namespace p2p
