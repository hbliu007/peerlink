#pragma once

#include <string>
#include <vector>
#include <optional>
#include <map>

namespace p2p {
namespace protocol {

/**
 * Protocol version structure
 */
struct ProtocolVersion {
    std::string protocol_id;  // e.g., "/libp2p/circuit/relay/0.2.0/hop"
    uint32_t major;
    uint32_t minor;
    uint32_t patch;

    ProtocolVersion() : major(0), minor(0), patch(0) {}
    ProtocolVersion(const std::string& id, uint32_t maj, uint32_t min, uint32_t p)
        : protocol_id(id), major(maj), minor(min), patch(p) {}

    std::string ToString() const;
    bool IsCompatibleWith(const ProtocolVersion& other) const;
    bool operator==(const ProtocolVersion& other) const;
    bool operator!=(const ProtocolVersion& other) const { return !(*this == other); }
    bool operator<(const ProtocolVersion& other) const;
    bool operator>(const ProtocolVersion& other) const { return other < *this; }
    bool operator<=(const ProtocolVersion& other) const { return !(other < *this); }
    bool operator>=(const ProtocolVersion& other) const { return !(*this < other); }
};

/**
 * Protocol negotiation result
 */
enum class NegotiationResult {
    SUCCESS,              // Negotiation successful
    VERSION_MISMATCH,     // Incompatible versions
    PROTOCOL_NOT_FOUND,   // Protocol not supported
    INVALID_VERSION,      // Invalid version format
    NEGOTIATION_FAILED    // General failure
};

/**
 * Protocol negotiation response
 */
struct NegotiationResponse {
    NegotiationResult result;
    std::optional<ProtocolVersion> negotiated_version;
    std::string error_message;

    NegotiationResponse(NegotiationResult res)
        : result(res) {}

    NegotiationResponse(NegotiationResult res, const ProtocolVersion& ver)
        : result(res), negotiated_version(ver) {}

    NegotiationResponse(NegotiationResult res, const std::string& err)
        : result(res), error_message(err) {}

    bool IsSuccess() const { return result == NegotiationResult::SUCCESS; }
};

/**
 * Protocol Negotiator
 *
 * Handles protocol version negotiation between peers.
 * Supports:
 * - Version declaration
 * - Version matching
 * - Backward compatibility
 * - Graceful degradation
 */
class ProtocolNegotiator {
public:
    ProtocolNegotiator();
    ~ProtocolNegotiator() = default;

    /**
     * Register a supported protocol version
     * @param version Protocol version to support
     */
    void RegisterProtocol(const ProtocolVersion& version);

    /**
     * Register multiple protocol versions
     * @param versions List of protocol versions
     */
    void RegisterProtocols(const std::vector<ProtocolVersion>& versions);

    /**
     * Negotiate protocol version with peer
     * @param peer_versions Versions supported by peer
     * @return Negotiation response with selected version
     */
    NegotiationResponse Negotiate(const std::vector<ProtocolVersion>& peer_versions);

    /**
     * Check if a specific protocol is supported
     * @param protocol_id Protocol identifier
     * @return true if supported
     */
    bool IsProtocolSupported(const std::string& protocol_id) const;

    /**
     * Get all supported versions for a protocol
     * @param protocol_id Protocol identifier
     * @return List of supported versions
     */
    std::vector<ProtocolVersion> GetSupportedVersions(const std::string& protocol_id) const;

    /**
     * Get all supported protocols
     * @return List of all supported protocol versions
     */
    std::vector<ProtocolVersion> GetAllSupportedProtocols() const;

    /**
     * Parse protocol version from string
     * @param version_str Version string (e.g., "/libp2p/circuit/relay/0.2.0/hop")
     * @return Parsed protocol version
     */
    static std::optional<ProtocolVersion> ParseVersion(const std::string& version_str);

    /**
     * Enable backward compatibility mode
     * When enabled, will accept older versions if major version matches
     */
    void EnableBackwardCompatibility(bool enable) { backward_compatible_ = enable; }

    /**
     * Check if backward compatibility is enabled
     */
    bool IsBackwardCompatible() const { return backward_compatible_; }

private:
    /**
     * Find best matching version
     * @param protocol_id Protocol identifier
     * @param peer_versions Peer's supported versions
     * @return Best matching version
     */
    std::optional<ProtocolVersion> FindBestMatch(
        const std::string& protocol_id,
        const std::vector<ProtocolVersion>& peer_versions) const;

    /**
     * Check if two versions are compatible
     * @param local Local version
     * @param peer Peer version
     * @return true if compatible
     */
    bool AreVersionsCompatible(const ProtocolVersion& local, const ProtocolVersion& peer) const;

    // Map: protocol_id -> list of supported versions (sorted by version)
    std::map<std::string, std::vector<ProtocolVersion>> supported_protocols_;

    bool backward_compatible_;
};

/**
 * Common protocol identifiers
 */
namespace ProtocolIDs {
    constexpr const char* DCUTR = "/libp2p/dcutr";
    constexpr const char* RELAY_V2_HOP = "/libp2p/circuit/relay/0.2.0/hop";
    constexpr const char* RELAY_V2_STOP = "/libp2p/circuit/relay/0.2.0/stop";
    constexpr const char* IDENTIFY = "/ipfs/id/1.0.0";
}

/**
 * Common protocol versions
 */
namespace CommonVersions {
    const ProtocolVersion DCUTR_V1{ProtocolIDs::DCUTR, 1, 0, 0};
    const ProtocolVersion RELAY_V2_HOP{ProtocolIDs::RELAY_V2_HOP, 0, 2, 0};
    const ProtocolVersion RELAY_V2_STOP{ProtocolIDs::RELAY_V2_STOP, 0, 2, 0};
}

}  // namespace protocol
}  // namespace p2p
