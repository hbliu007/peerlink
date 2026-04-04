#pragma once

#include <string>
#include <chrono>

namespace p2p {
namespace detection {

// Device vendor enumeration
enum class DeviceVendor {
    // Carrier-grade devices
    HUAWEI,
    ZTE,
    ERICSSON,
    NOKIA,
    FIBERHOME,
    ALCATEL_LUCENT,
    SAMSUNG,
    
    // Enterprise devices
    CISCO,
    H3C,
    SANGFOR,
    QIANXIN,
    PALO_ALTO,
    FORTINET,
    JUNIPER,
    CHECKPOINT,
    
    // Consumer devices
    TP_LINK,
    XIAOMI,
    
    // Unknown
    UNKNOWN
};

// NAT type
enum class NATType {
    PUBLIC,
    FULL_CONE,
    RESTRICTED_CONE,
    PORT_RESTRICTED,
    SYMMETRIC,
    UNKNOWN
};

// Port allocation strategy
enum class PortStrategy {
    CONTINUOUS,  // Sequential port allocation
    JUMP,        // Jump-style allocation
    RANDOM,      // Random allocation
    HYBRID       // Mixed strategy
};

// Punch strategy
enum class PunchStrategy {
    STANDARD,
    MULTI_PORT,
    MULTI_PORT_RETRY,
    TCP_FALLBACK,
    RELAY,
    RELAY_IMMEDIATELY,
    CHECK_POLICY
};

// Device profile
struct DeviceProfile {
    DeviceVendor vendor;
    
    // NAT behavior
    NATType nat_type;
    PortStrategy port_strategy;
    int port_delta;
    
    // Timeout configuration
    std::chrono::seconds udp_timeout;
    std::chrono::seconds tcp_timeout;
    
    // Special features
    bool alg_enabled;
    bool hairpin_supported;
    
    // Firewall behavior
    bool strict_inbound_filter;
    
    // Compatibility recommendations
    std::chrono::seconds heartbeat_interval;
    PunchStrategy punch_strategy;
    
    // Notes
    std::string notes;
};

// Utility functions
std::string to_string(DeviceVendor vendor);
std::string to_string(NATType nat_type);
std::string to_string(PortStrategy strategy);
std::string to_string(PunchStrategy strategy);

DeviceVendor device_vendor_from_string(const std::string& str);

} // namespace detection
} // namespace p2p
