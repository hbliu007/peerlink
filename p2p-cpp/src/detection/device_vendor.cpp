#include "p2p/detection/device_vendor.hpp"
#include <stdexcept>

namespace p2p {
namespace detection {

std::string to_string(DeviceVendor vendor) {
    switch (vendor) {
        case DeviceVendor::HUAWEI: return "Huawei";
        case DeviceVendor::ZTE: return "ZTE";
        case DeviceVendor::ERICSSON: return "Ericsson";
        case DeviceVendor::NOKIA: return "Nokia";
        case DeviceVendor::FIBERHOME: return "FiberHome";
        case DeviceVendor::ALCATEL_LUCENT: return "Alcatel-Lucent";
        case DeviceVendor::SAMSUNG: return "Samsung";
        case DeviceVendor::CISCO: return "Cisco";
        case DeviceVendor::H3C: return "H3C";
        case DeviceVendor::SANGFOR: return "Sangfor";
        case DeviceVendor::QIANXIN: return "Qianxin";
        case DeviceVendor::PALO_ALTO: return "Palo Alto";
        case DeviceVendor::FORTINET: return "Fortinet";
        case DeviceVendor::JUNIPER: return "Juniper";
        case DeviceVendor::CHECKPOINT: return "Check Point";
        case DeviceVendor::TP_LINK: return "TP-Link";
        case DeviceVendor::XIAOMI: return "Xiaomi";
        case DeviceVendor::UNKNOWN: return "Unknown";
        default: return "Unknown";
    }
}

std::string to_string(NATType nat_type) {
    switch (nat_type) {
        case NATType::PUBLIC: return "Public";
        case NATType::FULL_CONE: return "Full Cone";
        case NATType::RESTRICTED_CONE: return "Restricted Cone";
        case NATType::PORT_RESTRICTED: return "Port Restricted";
        case NATType::SYMMETRIC: return "Symmetric";
        case NATType::UNKNOWN: return "Unknown";
        default: return "Unknown";
    }
}

std::string to_string(PortStrategy strategy) {
    switch (strategy) {
        case PortStrategy::CONTINUOUS: return "Continuous";
        case PortStrategy::JUMP: return "Jump";
        case PortStrategy::RANDOM: return "Random";
        case PortStrategy::HYBRID: return "Hybrid";
        default: return "Unknown";
    }
}

std::string to_string(PunchStrategy strategy) {
    switch (strategy) {
        case PunchStrategy::STANDARD: return "Standard";
        case PunchStrategy::MULTI_PORT: return "Multi-Port";
        case PunchStrategy::MULTI_PORT_RETRY: return "Multi-Port Retry";
        case PunchStrategy::TCP_FALLBACK: return "TCP Fallback";
        case PunchStrategy::RELAY: return "Relay";
        case PunchStrategy::RELAY_IMMEDIATELY: return "Relay Immediately";
        case PunchStrategy::CHECK_POLICY: return "Check Policy";
        default: return "Unknown";
    }
}

DeviceVendor device_vendor_from_string(const std::string& str) {
    if (str == "Huawei") return DeviceVendor::HUAWEI;
    if (str == "ZTE") return DeviceVendor::ZTE;
    if (str == "Ericsson") return DeviceVendor::ERICSSON;
    if (str == "Nokia") return DeviceVendor::NOKIA;
    if (str == "FiberHome") return DeviceVendor::FIBERHOME;
    if (str == "Alcatel-Lucent") return DeviceVendor::ALCATEL_LUCENT;
    if (str == "Samsung") return DeviceVendor::SAMSUNG;
    if (str == "Cisco") return DeviceVendor::CISCO;
    if (str == "H3C") return DeviceVendor::H3C;
    if (str == "Sangfor") return DeviceVendor::SANGFOR;
    if (str == "Qianxin") return DeviceVendor::QIANXIN;
    if (str == "Palo Alto") return DeviceVendor::PALO_ALTO;
    if (str == "Fortinet") return DeviceVendor::FORTINET;
    if (str == "Juniper") return DeviceVendor::JUNIPER;
    if (str == "Check Point") return DeviceVendor::CHECKPOINT;
    if (str == "TP-Link") return DeviceVendor::TP_LINK;
    if (str == "Xiaomi") return DeviceVendor::XIAOMI;
    return DeviceVendor::UNKNOWN;
}

} // namespace detection
} // namespace p2p
