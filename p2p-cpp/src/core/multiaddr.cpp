#include "p2p/core/types.hpp"
#include "p2p/core/varint.hpp"
#include <sstream>
#include <arpa/inet.h>
#include <cstring>
#include <algorithm>

namespace p2p {

// Protocol codes (from multicodec table)
enum class ProtocolCode : uint32_t {
    IP4 = 4,
    TCP = 6,
    UDP = 273,
    IP6 = 41,
    P2P = 421
};

// Protocol metadata
struct ProtocolInfo {
    ProtocolCode code;
    std::string name;
    size_t addr_size;  // 0 = variable length
};

static const ProtocolInfo kProtocols[] = {
    {ProtocolCode::IP4, "ip4", 4},
    {ProtocolCode::TCP, "tcp", 2},
    {ProtocolCode::UDP, "udp", 2},
    {ProtocolCode::IP6, "ip6", 16},
    {ProtocolCode::P2P, "p2p", 0}  // Variable length (base58 peer ID)
};

static const ProtocolInfo* FindProtocol(const std::string& name) {
    for (const auto& proto : kProtocols) {
        if (proto.name == name) {
            return &proto;
        }
    }
    return nullptr;
}

static const ProtocolInfo* FindProtocol(ProtocolCode code) {
    for (const auto& proto : kProtocols) {
        if (proto.code == code) {
            return &proto;
        }
    }
    return nullptr;
}

// Parse IPv4 address string to 4 bytes
static bool ParseIPv4(const std::string& addr_str, std::vector<uint8_t>& out) {
    struct in_addr addr;
    if (inet_pton(AF_INET, addr_str.c_str(), &addr) != 1) {
        return false;
    }
    out.resize(4);
    std::memcpy(out.data(), &addr.s_addr, 4);
    return true;
}

// Parse IPv6 address string to 16 bytes
static bool ParseIPv6(const std::string& addr_str, std::vector<uint8_t>& out) {
    struct in6_addr addr;
    if (inet_pton(AF_INET6, addr_str.c_str(), &addr) != 1) {
        return false;
    }
    out.resize(16);
    std::memcpy(out.data(), &addr.s6_addr, 16);
    return true;
}

// Parse port number to 2 bytes (big-endian)
static bool ParsePort(const std::string& port_str, std::vector<uint8_t>& out) {
    try {
        int port = std::stoi(port_str);
        if (port < 0 || port > 65535) {
            return false;
        }
        out.resize(2);
        out[0] = (port >> 8) & 0xFF;  // High byte
        out[1] = port & 0xFF;          // Low byte
        return true;
    } catch (...) {
        return false;
    }
}

// Parse peer ID (base58 string) - for now just store as-is
static bool ParsePeerID(const std::string& peer_id, std::vector<uint8_t>& out) {
    // Simplified: just store the string bytes
    // TODO: proper base58 decoding
    if (peer_id.empty() || peer_id.size() > 255) {
        return false;
    }
    out.assign(peer_id.begin(), peer_id.end());
    return true;
}

Status Multiaddr::Parse() {
    if (addr_.empty() || addr_[0] != '/') {
        return Status::Error(StatusCode::ERROR_INVALID_ARGUMENT,
                           "Multiaddr must start with '/'");
    }

    bytes_.clear();
    std::istringstream iss(addr_.substr(1));  // Skip leading '/'
    std::string component;

    while (std::getline(iss, component, '/')) {
        if (component.empty()) {
            continue;
        }

        // Find protocol
        const ProtocolInfo* proto = FindProtocol(component);
        if (!proto) {
            return Status::Error(StatusCode::ERROR_INVALID_ARGUMENT,
                               "Unknown protocol: " + component);
        }

        // Encode protocol code
        auto code_bytes = varint::Encode(static_cast<uint64_t>(proto->code));
        bytes_.insert(bytes_.end(), code_bytes.begin(), code_bytes.end());

        // Parse and encode address
        if (proto->addr_size > 0) {
            // Fixed-size address, read next component
            if (!std::getline(iss, component, '/')) {
                return Status::Error(StatusCode::ERROR_INVALID_ARGUMENT,
                                   "Missing address for protocol: " + proto->name);
            }

            std::vector<uint8_t> addr_bytes;
            bool success = false;

            if (proto->code == ProtocolCode::IP4) {
                success = ParseIPv4(component, addr_bytes);
            } else if (proto->code == ProtocolCode::IP6) {
                success = ParseIPv6(component, addr_bytes);
            } else if (proto->code == ProtocolCode::TCP || proto->code == ProtocolCode::UDP) {
                success = ParsePort(component, addr_bytes);
            }

            if (!success) {
                return Status::Error(StatusCode::ERROR_INVALID_ARGUMENT,
                                   "Invalid address for protocol " + proto->name + ": " + component);
            }

            bytes_.insert(bytes_.end(), addr_bytes.begin(), addr_bytes.end());
        } else {
            // Variable-size address (p2p peer ID)
            if (!std::getline(iss, component, '/')) {
                return Status::Error(StatusCode::ERROR_INVALID_ARGUMENT,
                                   "Missing peer ID for p2p protocol");
            }

            std::vector<uint8_t> peer_id_bytes;
            if (!ParsePeerID(component, peer_id_bytes)) {
                return Status::Error(StatusCode::ERROR_INVALID_ARGUMENT,
                                   "Invalid peer ID: " + component);
            }

            // Encode length + data
            auto len_bytes = varint::Encode(peer_id_bytes.size());
            bytes_.insert(bytes_.end(), len_bytes.begin(), len_bytes.end());
            bytes_.insert(bytes_.end(), peer_id_bytes.begin(), peer_id_bytes.end());
        }
    }

    parsed_ = true;
    return Status::OK();
}

std::vector<uint8_t> Multiaddr::ToBytes() const {
    return bytes_;
}

// Format IPv4 bytes to string
static std::string FormatIPv4(const uint8_t* data) {
    char buf[INET_ADDRSTRLEN];
    struct in_addr addr;
    std::memcpy(&addr.s_addr, data, 4);
    inet_ntop(AF_INET, &addr, buf, sizeof(buf));
    return std::string(buf);
}

// Format IPv6 bytes to string
static std::string FormatIPv6(const uint8_t* data) {
    char buf[INET6_ADDRSTRLEN];
    struct in6_addr addr;
    std::memcpy(&addr.s6_addr, data, 16);
    inet_ntop(AF_INET6, &addr, buf, sizeof(buf));
    return std::string(buf);
}

// Format port bytes to string
static std::string FormatPort(const uint8_t* data) {
    uint16_t port = (static_cast<uint16_t>(data[0]) << 8) | data[1];
    return std::to_string(port);
}

std::optional<Multiaddr> Multiaddr::FromBytes(const std::vector<uint8_t>& bytes) {
    if (bytes.empty()) {
        return std::nullopt;
    }

    std::string addr_str;
    size_t offset = 0;

    while (offset < bytes.size()) {
        // Decode protocol code
        auto [code_val, consumed] = varint::Decode(bytes.data() + offset, bytes.size() - offset);
        if (consumed == 0) {
            return std::nullopt;
        }
        offset += consumed;

        const ProtocolInfo* proto = FindProtocol(static_cast<ProtocolCode>(code_val));
        if (!proto) {
            return std::nullopt;
        }

        addr_str += "/" + proto->name;

        // Decode address
        if (proto->addr_size > 0) {
            // Fixed-size address
            if (offset + proto->addr_size > bytes.size()) {
                return std::nullopt;
            }

            std::string addr_component;
            if (proto->code == ProtocolCode::IP4) {
                addr_component = FormatIPv4(bytes.data() + offset);
            } else if (proto->code == ProtocolCode::IP6) {
                addr_component = FormatIPv6(bytes.data() + offset);
            } else if (proto->code == ProtocolCode::TCP || proto->code == ProtocolCode::UDP) {
                addr_component = FormatPort(bytes.data() + offset);
            }

            addr_str += "/" + addr_component;
            offset += proto->addr_size;
        } else {
            // Variable-size address (p2p peer ID)
            auto [len, len_consumed] = varint::Decode(bytes.data() + offset, bytes.size() - offset);
            if (len_consumed == 0 || offset + len_consumed + len > bytes.size()) {
                return std::nullopt;
            }
            offset += len_consumed;

            std::string peer_id(bytes.begin() + offset, bytes.begin() + offset + len);
            addr_str += "/" + peer_id;
            offset += len;
        }
    }

    Multiaddr result(addr_str);
    result.bytes_ = bytes;
    result.parsed_ = true;
    return result;
}

}  // namespace p2p
