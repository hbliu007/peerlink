#pragma once

#include <boost/asio.hpp>
#include <string>
#include <cstdint>
#include <optional>
#include <memory>

namespace p2p {
namespace nat {

/**
 * @brief NAT type classification according to RFC 3489
 */
enum class NATType {
    PUBLIC_IP,           // No NAT, device has public IP
    FULL_CONE,          // Full Cone NAT
    RESTRICTED_CONE,    // Restricted Cone NAT
    PORT_RESTRICTED_CONE, // Port Restricted Cone NAT
    SYMMETRIC,          // Symmetric NAT
    UNKNOWN,            // Could not determine
    BLOCKED             // UDP is blocked
};

/**
 * @brief Result of NAT detection
 */
struct NATDetectionResult {
    NATType nat_type;
    std::optional<std::string> public_ip;
    std::optional<uint16_t> public_port;
    std::optional<std::string> local_ip;
    std::optional<uint16_t> local_port;
};

/**
 * @brief STUN protocol client for NAT detection
 *
 * Implements a subset of RFC 5389 (STUN) for NAT type discovery.
 */
class STUNClient {
public:
    /**
     * @brief Construct STUN client
     * @param io_context Boost.Asio IO context
     * @param stun_server STUN server hostname or IP
     * @param stun_port STUN server port (default 3478)
     */
    STUNClient(boost::asio::io_context& io_context,
               const std::string& stun_server,
               uint16_t stun_port = 3478);

    /**
     * @brief Send STUN binding request and get mapped address
     * @param callback Completion callback with result
     */
    void send_request(std::function<void(
        std::optional<std::string> public_ip,
        std::optional<uint16_t> public_port
    )> callback);

#ifdef TESTING
    // Test-only public interface
    std::vector<uint8_t> packStunRequest();
    std::pair<std::optional<std::string>, std::optional<uint16_t>>
        unpackStunResponse(const std::vector<uint8_t>& data);
#endif

private:
#ifndef TESTING
    std::vector<uint8_t> packStunRequest();
    std::pair<std::optional<std::string>, std::optional<uint16_t>>
        unpackStunResponse(const std::vector<uint8_t>& data);
#endif


    boost::asio::io_context& io_context_;
    std::string stun_server_;
    uint16_t stun_port_;
    std::chrono::seconds timeout_;

    // STUN message types
    static constexpr uint16_t BINDING_REQUEST = 0x0001;
    static constexpr uint16_t BINDING_RESPONSE = 0x0101;

    // STUN attributes
    static constexpr uint16_t ATTR_MAPPED_ADDRESS = 0x0001;
    static constexpr uint16_t ATTR_XOR_MAPPED_ADDRESS = 0x0020;
};

/**
 * @brief Detect NAT type using STUN server
 * @param io_context Boost.Asio IO context
 * @param stun_server STUN server hostname
 * @param stun_port STUN server port
 * @param callback Completion callback with detection result
 */
void detect_nat_type(
    boost::asio::io_context& io_context,
    const std::string& stun_server,
    uint16_t stun_port,
    std::function<void(const NATDetectionResult&)> callback
);

/**
 * @brief Check if NAT type is capable of P2P connection
 * @param nat_type The detected NAT type
 * @return True if P2P is possible, false if relay is needed
 */
bool is_nat_p2p_capable(NATType nat_type);

} // namespace nat
} // namespace p2p
