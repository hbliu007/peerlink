#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include "p2p/net/socket.hpp"

namespace p2p {
namespace nat {

/**
 * Multiaddr to SocketAddr converter
 * Converts DCUtR Address (multiaddr bytes) to net::SocketAddr for socket operations
 */
class MultiaddrConverter {
public:
    /**
     * Parse multiaddr bytes and extract IP:port for UDP
     * @param addr_bytes Multiaddr as bytes (e.g., /ip4/1.2.3.4/udp/1234)
     * @return SocketAddr if valid UDP address found
     */
    static std::optional<net::SocketAddr> parse_udp(const std::vector<uint8_t>& addr_bytes);

    /**
     * Parse multiaddr bytes and extract IP:port for TCP
     * @param addr_bytes Multiaddr as bytes (e.g., /ip4/1.2.3.4/tcp/1234)
     * @return SocketAddr if valid TCP address found
     */
    static std::optional<net::SocketAddr> parse_tcp(const std::vector<uint8_t>& addr_bytes);

    /**
     * Parse multiaddr and return both IP and transport type
     * @param addr_bytes Multiaddr as bytes
     * @return Pair of (transport_type, SocketAddr) if valid
     */
    static std::optional<std::pair<std::string, net::SocketAddr>> parse(
        const std::vector<uint8_t>& addr_bytes);

private:
    /**
     * Parse varint from byte array
     */
    static std::pair<uint64_t, size_t> decode_varint(
        const uint8_t* data, size_t size);

    /**
     * Format IPv4 bytes to string
     */
    static std::string format_ipv4(const uint8_t* data);

    /**
     * Format IPv6 bytes to string
     */
    static std::string format_ipv6(const uint8_t* data);

    /**
     * Parse port from 2 bytes (big-endian)
     */
    static uint16_t parse_port(const uint8_t* data);
};

} // namespace nat
} // namespace p2p
