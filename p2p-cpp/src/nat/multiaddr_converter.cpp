#include "p2p/nat/multiaddr_converter.hpp"
#include <arpa/inet.h>
#include <cstring>

namespace p2p {
namespace nat {

// Protocol codes (from multicodec table)
enum class ProtocolCode : uint32_t {
    IP4 = 4,
    TCP = 6,
    UDP = 273,
    IP6 = 41,
    DNSADDR = 56,
    P2P = 421
};

std::pair<uint64_t, size_t> MultiaddrConverter::decode_varint(
    const uint8_t* data, size_t size) {

    uint64_t value = 0;
    size_t offset = 0;

    while (offset < size) {
        uint8_t byte = data[offset];
        value |= static_cast<uint64_t>(byte & 0x7F) << (7 * offset);
        offset++;

        if (!(byte & 0x80)) {
            break;
        }

        if (offset >= 10) {
            // Varint too long
            return {0, 0};
        }
    }

    return {value, offset};
}

std::string MultiaddrConverter::format_ipv4(const uint8_t* data) {
    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, data, buf, sizeof(buf));
    return std::string(buf);
}

std::string MultiaddrConverter::format_ipv6(const uint8_t* data) {
    char buf[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, data, buf, sizeof(buf));
    return std::string(buf);
}

uint16_t MultiaddrConverter::parse_port(const uint8_t* data) {
    return (static_cast<uint16_t>(data[0]) << 8) | data[1];
}

std::optional<net::SocketAddr> MultiaddrConverter::parse_udp(
    const std::vector<uint8_t>& addr_bytes) {

    auto result = parse(addr_bytes);
    if (result && result->first == "udp") {
        return result->second;
    }
    return std::nullopt;
}

std::optional<net::SocketAddr> MultiaddrConverter::parse_tcp(
    const std::vector<uint8_t>& addr_bytes) {

    auto result = parse(addr_bytes);
    if (result && result->first == "tcp") {
        return result->second;
    }
    return std::nullopt;
}

std::optional<std::pair<std::string, net::SocketAddr>> MultiaddrConverter::parse(
    const std::vector<uint8_t>& addr_bytes) {

    if (addr_bytes.empty()) {
        return std::nullopt;
    }

    size_t offset = 0;
    std::string ip;
    std::string transport;
    uint16_t port = 0;
    bool found_ip = false;
    bool found_port = false;

    while (offset < addr_bytes.size()) {
        // Decode protocol code
        auto [code_val, consumed] = MultiaddrConverter::decode_varint(
            addr_bytes.data() + offset,
            addr_bytes.size() - offset);

        if (consumed == 0) {
            return std::nullopt;
        }
        offset += consumed;

        auto proto_code = static_cast<ProtocolCode>(code_val);

        // Handle protocol-specific data
        switch (proto_code) {
            case ProtocolCode::IP4:
                if (offset + 4 > addr_bytes.size()) {
                    return std::nullopt;
                }
                ip = format_ipv4(addr_bytes.data() + offset);
                offset += 4;
                found_ip = true;
                break;

            case ProtocolCode::IP6:
                if (offset + 16 > addr_bytes.size()) {
                    return std::nullopt;
                }
                ip = format_ipv6(addr_bytes.data() + offset);
                offset += 16;
                found_ip = true;
                break;

            case ProtocolCode::UDP:
                transport = "udp";
                if (offset + 2 > addr_bytes.size()) {
                    return std::nullopt;
                }
                port = parse_port(addr_bytes.data() + offset);
                offset += 2;
                found_port = true;
                break;

            case ProtocolCode::TCP:
                transport = "tcp";
                if (offset + 2 > addr_bytes.size()) {
                    return std::nullopt;
                }
                port = parse_port(addr_bytes.data() + offset);
                offset += 2;
                found_port = true;
                break;

            case ProtocolCode::P2P:
            case ProtocolCode::DNSADDR:
                // Skip variable-length data
                {
                    auto [len, len_consumed] = decode_varint(
                        addr_bytes.data() + offset,
                        addr_bytes.size() - offset);
                    if (len_consumed == 0 || offset + len_consumed + len > addr_bytes.size()) {
                        return std::nullopt;
                    }
                    offset += len_consumed + len;
                }
                break;

            default:
                // Unknown protocol, try to skip
                // Assume varint length follows
                {
                    auto [len, len_consumed] = decode_varint(
                        addr_bytes.data() + offset,
                        addr_bytes.size() - offset);
                    if (len_consumed == 0) {
                        // Can't determine length, abort
                        return std::nullopt;
                    }
                    offset += len_consumed + len;
                }
                break;
        }

        // Check if we have both IP and port
        if (found_ip && found_port && !transport.empty()) {
            return std::make_pair(transport, net::SocketAddr(ip, port));
        }
    }

    return std::nullopt;
}

} // namespace nat
} // namespace p2p
