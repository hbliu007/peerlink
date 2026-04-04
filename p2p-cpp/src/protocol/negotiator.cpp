#include "p2p/protocol/negotiator.hpp"
#include <algorithm>
#include <sstream>
#include <regex>

namespace p2p {
namespace protocol {

// ProtocolVersion implementation
std::string ProtocolVersion::ToString() const {
    std::ostringstream oss;
    oss << protocol_id << "/" << major << "." << minor << "." << patch;
    return oss.str();
}

bool ProtocolVersion::IsCompatibleWith(const ProtocolVersion& other) const {
    // Same protocol ID required
    if (protocol_id != other.protocol_id) {
        return false;
    }

    // Major version must match for compatibility
    if (major != other.major) {
        return false;
    }

    // Minor version backward compatible (higher can work with lower)
    // Patch version always compatible
    return true;
}

bool ProtocolVersion::operator==(const ProtocolVersion& other) const {
    return protocol_id == other.protocol_id &&
           major == other.major &&
           minor == other.minor &&
           patch == other.patch;
}

bool ProtocolVersion::operator<(const ProtocolVersion& other) const {
    if (protocol_id != other.protocol_id) {
        return protocol_id < other.protocol_id;
    }
    if (major != other.major) {
        return major < other.major;
    }
    if (minor != other.minor) {
        return minor < other.minor;
    }
    return patch < other.patch;
}

// ProtocolNegotiator implementation
ProtocolNegotiator::ProtocolNegotiator()
    : backward_compatible_(true) {
}

void ProtocolNegotiator::RegisterProtocol(const ProtocolVersion& version) {
    auto& versions = supported_protocols_[version.protocol_id];

    // Check if already registered
    auto it = std::find(versions.begin(), versions.end(), version);
    if (it == versions.end()) {
        versions.push_back(version);
        // Keep sorted (highest version first)
        std::sort(versions.begin(), versions.end(), std::greater<ProtocolVersion>());
    }
}

void ProtocolNegotiator::RegisterProtocols(const std::vector<ProtocolVersion>& versions) {
    for (const auto& version : versions) {
        RegisterProtocol(version);
    }
}

NegotiationResponse ProtocolNegotiator::Negotiate(
    const std::vector<ProtocolVersion>& peer_versions) {

    if (peer_versions.empty()) {
        return NegotiationResponse(NegotiationResult::INVALID_VERSION,
                                   "Peer provided no protocol versions");
    }

    // Try to find a matching protocol
    for (const auto& peer_version : peer_versions) {
        auto match = FindBestMatch(peer_version.protocol_id, peer_versions);
        if (match.has_value()) {
            return NegotiationResponse(NegotiationResult::SUCCESS, *match);
        }
    }

    // No compatible protocol found
    return NegotiationResponse(NegotiationResult::VERSION_MISMATCH,
                               "No compatible protocol version found");
}

bool ProtocolNegotiator::IsProtocolSupported(const std::string& protocol_id) const {
    return supported_protocols_.find(protocol_id) != supported_protocols_.end();
}

std::vector<ProtocolVersion> ProtocolNegotiator::GetSupportedVersions(
    const std::string& protocol_id) const {

    auto it = supported_protocols_.find(protocol_id);
    if (it != supported_protocols_.end()) {
        return it->second;
    }
    return {};
}

std::vector<ProtocolVersion> ProtocolNegotiator::GetAllSupportedProtocols() const {
    std::vector<ProtocolVersion> all_protocols;
    for (const auto& [protocol_id, versions] : supported_protocols_) {
        all_protocols.insert(all_protocols.end(), versions.begin(), versions.end());
    }
    return all_protocols;
}

std::optional<ProtocolVersion> ProtocolNegotiator::ParseVersion(const std::string& version_str) {
    // Expected format: /protocol/name/major.minor.patch/subprotocol
    // Example: /libp2p/circuit/relay/0.2.0/hop

    // Simple parsing: extract version numbers
    std::regex version_regex(R"((\d+)\.(\d+)\.(\d+))");
    std::smatch match;

    if (std::regex_search(version_str, match, version_regex)) {
        if (match.size() == 4) {
            ProtocolVersion version;
            version.protocol_id = version_str;
            version.major = std::stoul(match[1].str());
            version.minor = std::stoul(match[2].str());
            version.patch = std::stoul(match[3].str());
            return version;
        }
    }

    // If no version numbers found, treat as version 1.0.0
    ProtocolVersion version;
    version.protocol_id = version_str;
    version.major = 1;
    version.minor = 0;
    version.patch = 0;
    return version;
}

std::optional<ProtocolVersion> ProtocolNegotiator::FindBestMatch(
    const std::string& protocol_id,
    const std::vector<ProtocolVersion>& peer_versions) const {

    // Get our supported versions for this protocol
    auto local_versions = GetSupportedVersions(protocol_id);
    if (local_versions.empty()) {
        return std::nullopt;
    }

    // Find best matching version
    // Prefer exact match, then highest compatible version
    for (const auto& local_version : local_versions) {
        for (const auto& peer_version : peer_versions) {
            if (peer_version.protocol_id != protocol_id) {
                continue;
            }

            // Exact match
            if (local_version == peer_version) {
                return local_version;
            }

            // Compatible match
            if (AreVersionsCompatible(local_version, peer_version)) {
                return local_version;
            }
        }
    }

    return std::nullopt;
}

bool ProtocolNegotiator::AreVersionsCompatible(
    const ProtocolVersion& local,
    const ProtocolVersion& peer) const {

    if (!backward_compatible_) {
        // Strict mode: exact match required
        return local == peer;
    }

    // Backward compatible mode
    return local.IsCompatibleWith(peer);
}

}  // namespace protocol
}  // namespace p2p
