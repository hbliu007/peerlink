#include "p2p/detection/device_detector.hpp"
#include <algorithm>
#include <cmath>

using namespace std::chrono_literals;

namespace p2p {
namespace detection {

DeviceDetector::DeviceDetector() {
    initialize_profiles();
}

void DeviceDetector::initialize_profiles() {
    // Huawei - Carrier-grade NAT device
    profiles_[DeviceVendor::HUAWEI] = {
        .vendor = DeviceVendor::HUAWEI,
        .nat_type = NATType::PORT_RESTRICTED,
        .port_strategy = PortStrategy::CONTINUOUS,
        .port_delta = 1,
        .udp_timeout = 300s,
        .tcp_timeout = 7200s,
        .alg_enabled = true,
        .hairpin_supported = false,
        .strict_inbound_filter = true,
        .heartbeat_interval = 60s,
        .punch_strategy = PunchStrategy::MULTI_PORT_RETRY,
        .notes = "Huawei carrier-grade NAT with strict filtering"
    };

    // ZTE - Carrier-grade NAT device
    profiles_[DeviceVendor::ZTE] = {
        .vendor = DeviceVendor::ZTE,
        .nat_type = NATType::PORT_RESTRICTED,
        .port_strategy = PortStrategy::CONTINUOUS,
        .port_delta = 1,
        .udp_timeout = 300s,
        .tcp_timeout = 7200s,
        .alg_enabled = true,
        .hairpin_supported = false,
        .strict_inbound_filter = true,
        .heartbeat_interval = 60s,
        .punch_strategy = PunchStrategy::MULTI_PORT_RETRY,
        .notes = "ZTE carrier-grade NAT, similar to Huawei"
    };

    // Ericsson - Carrier-grade NAT device
    profiles_[DeviceVendor::ERICSSON] = {
        .vendor = DeviceVendor::ERICSSON,
        .nat_type = NATType::SYMMETRIC,
        .port_strategy = PortStrategy::RANDOM,
        .port_delta = 0,
        .udp_timeout = 180s,
        .tcp_timeout = 3600s,
        .alg_enabled = false,
        .hairpin_supported = false,
        .strict_inbound_filter = true,
        .heartbeat_interval = 45s,
        .punch_strategy = PunchStrategy::RELAY,
        .notes = "Ericsson symmetric NAT, difficult for P2P"
    };

    // Nokia - Carrier-grade NAT device
    profiles_[DeviceVendor::NOKIA] = {
        .vendor = DeviceVendor::NOKIA,
        .nat_type = NATType::SYMMETRIC,
        .port_strategy = PortStrategy::RANDOM,
        .port_delta = 0,
        .udp_timeout = 180s,
        .tcp_timeout = 3600s,
        .alg_enabled = false,
        .hairpin_supported = false,
        .strict_inbound_filter = true,
        .heartbeat_interval = 45s,
        .punch_strategy = PunchStrategy::RELAY,
        .notes = "Nokia symmetric NAT, similar to Ericsson"
    };

    // FiberHome - Carrier-grade NAT device
    profiles_[DeviceVendor::FIBERHOME] = {
        .vendor = DeviceVendor::FIBERHOME,
        .nat_type = NATType::PORT_RESTRICTED,
        .port_strategy = PortStrategy::CONTINUOUS,
        .port_delta = 2,
        .udp_timeout = 240s,
        .tcp_timeout = 3600s,
        .alg_enabled = true,
        .hairpin_supported = false,
        .strict_inbound_filter = true,
        .heartbeat_interval = 60s,
        .punch_strategy = PunchStrategy::MULTI_PORT,
        .notes = "FiberHome with moderate filtering"
    };

    // Alcatel-Lucent - Carrier-grade NAT device
    profiles_[DeviceVendor::ALCATEL_LUCENT] = {
        .vendor = DeviceVendor::ALCATEL_LUCENT,
        .nat_type = NATType::SYMMETRIC,
        .port_strategy = PortStrategy::HYBRID,
        .port_delta = 0,
        .udp_timeout = 200s,
        .tcp_timeout = 3600s,
        .alg_enabled = false,
        .hairpin_supported = false,
        .strict_inbound_filter = true,
        .heartbeat_interval = 50s,
        .punch_strategy = PunchStrategy::RELAY,
        .notes = "Alcatel-Lucent symmetric NAT"
    };

    // Samsung - Carrier-grade NAT device
    profiles_[DeviceVendor::SAMSUNG] = {
        .vendor = DeviceVendor::SAMSUNG,
        .nat_type = NATType::PORT_RESTRICTED,
        .port_strategy = PortStrategy::CONTINUOUS,
        .port_delta = 1,
        .udp_timeout = 300s,
        .tcp_timeout = 7200s,
        .alg_enabled = true,
        .hairpin_supported = false,
        .strict_inbound_filter = false,
        .heartbeat_interval = 60s,
        .punch_strategy = PunchStrategy::STANDARD,
        .notes = "Samsung with moderate NAT behavior"
    };

    // Cisco - Enterprise firewall/NAT
    profiles_[DeviceVendor::CISCO] = {
        .vendor = DeviceVendor::CISCO,
        .nat_type = NATType::PORT_RESTRICTED,
        .port_strategy = PortStrategy::CONTINUOUS,
        .port_delta = 1,
        .udp_timeout = 240s,
        .tcp_timeout = 3600s,
        .alg_enabled = true,
        .hairpin_supported = true,
        .strict_inbound_filter = true,
        .heartbeat_interval = 60s,
        .punch_strategy = PunchStrategy::STANDARD,
        .notes = "Cisco enterprise NAT with ALG support"
    };

    // H3C - Enterprise firewall/NAT
    profiles_[DeviceVendor::H3C] = {
        .vendor = DeviceVendor::H3C,
        .nat_type = NATType::PORT_RESTRICTED,
        .port_strategy = PortStrategy::CONTINUOUS,
        .port_delta = 1,
        .udp_timeout = 300s,
        .tcp_timeout = 3600s,
        .alg_enabled = true,
        .hairpin_supported = true,
        .strict_inbound_filter = false,
        .heartbeat_interval = 60s,
        .punch_strategy = PunchStrategy::STANDARD,
        .notes = "H3C enterprise NAT, similar to Cisco"
    };

    // Sangfor - Enterprise firewall/NAT
    profiles_[DeviceVendor::SANGFOR] = {
        .vendor = DeviceVendor::SANGFOR,
        .nat_type = NATType::SYMMETRIC,
        .port_strategy = PortStrategy::RANDOM,
        .port_delta = 0,
        .udp_timeout = 180s,
        .tcp_timeout = 3600s,
        .alg_enabled = false,
        .hairpin_supported = false,
        .strict_inbound_filter = true,
        .heartbeat_interval = 45s,
        .punch_strategy = PunchStrategy::CHECK_POLICY,
        .notes = "Sangfor strict security policy"
    };

    // Qianxin - Enterprise firewall/NAT
    profiles_[DeviceVendor::QIANXIN] = {
        .vendor = DeviceVendor::QIANXIN,
        .nat_type = NATType::SYMMETRIC,
        .port_strategy = PortStrategy::RANDOM,
        .port_delta = 0,
        .udp_timeout = 180s,
        .tcp_timeout = 3600s,
        .alg_enabled = false,
        .hairpin_supported = false,
        .strict_inbound_filter = true,
        .heartbeat_interval = 45s,
        .punch_strategy = PunchStrategy::CHECK_POLICY,
        .notes = "Qianxin strict security policy"
    };

    // Palo Alto - Enterprise firewall/NAT
    profiles_[DeviceVendor::PALO_ALTO] = {
        .vendor = DeviceVendor::PALO_ALTO,
        .nat_type = NATType::PORT_RESTRICTED,
        .port_strategy = PortStrategy::CONTINUOUS,
        .port_delta = 1,
        .udp_timeout = 300s,
        .tcp_timeout = 3600s,
        .alg_enabled = true,
        .hairpin_supported = true,
        .strict_inbound_filter = true,
        .heartbeat_interval = 60s,
        .punch_strategy = PunchStrategy::STANDARD,
        .notes = "Palo Alto enterprise firewall"
    };

    // Fortinet - Enterprise firewall/NAT
    profiles_[DeviceVendor::FORTINET] = {
        .vendor = DeviceVendor::FORTINET,
        .nat_type = NATType::PORT_RESTRICTED,
        .port_strategy = PortStrategy::CONTINUOUS,
        .port_delta = 1,
        .udp_timeout = 300s,
        .tcp_timeout = 3600s,
        .alg_enabled = true,
        .hairpin_supported = true,
        .strict_inbound_filter = true,
        .heartbeat_interval = 60s,
        .punch_strategy = PunchStrategy::STANDARD,
        .notes = "Fortinet enterprise firewall"
    };

    // Juniper - Enterprise firewall/NAT
    profiles_[DeviceVendor::JUNIPER] = {
        .vendor = DeviceVendor::JUNIPER,
        .nat_type = NATType::PORT_RESTRICTED,
        .port_strategy = PortStrategy::CONTINUOUS,
        .port_delta = 1,
        .udp_timeout = 240s,
        .tcp_timeout = 3600s,
        .alg_enabled = true,
        .hairpin_supported = true,
        .strict_inbound_filter = true,
        .heartbeat_interval = 60s,
        .punch_strategy = PunchStrategy::STANDARD,
        .notes = "Juniper enterprise firewall"
    };

    // Check Point - Enterprise firewall/NAT
    profiles_[DeviceVendor::CHECKPOINT] = {
        .vendor = DeviceVendor::CHECKPOINT,
        .nat_type = NATType::PORT_RESTRICTED,
        .port_strategy = PortStrategy::CONTINUOUS,
        .port_delta = 1,
        .udp_timeout = 300s,
        .tcp_timeout = 3600s,
        .alg_enabled = true,
        .hairpin_supported = true,
        .strict_inbound_filter = true,
        .heartbeat_interval = 60s,
        .punch_strategy = PunchStrategy::STANDARD,
        .notes = "Check Point enterprise firewall"
    };

    // TP-Link - Consumer router
    profiles_[DeviceVendor::TP_LINK] = {
        .vendor = DeviceVendor::TP_LINK,
        .nat_type = NATType::FULL_CONE,
        .port_strategy = PortStrategy::CONTINUOUS,
        .port_delta = 1,
        .udp_timeout = 600s,
        .tcp_timeout = 7200s,
        .alg_enabled = true,
        .hairpin_supported = true,
        .strict_inbound_filter = false,
        .heartbeat_interval = 120s,
        .punch_strategy = PunchStrategy::STANDARD,
        .notes = "TP-Link consumer router, P2P friendly"
    };

    // Xiaomi - Consumer router
    profiles_[DeviceVendor::XIAOMI] = {
        .vendor = DeviceVendor::XIAOMI,
        .nat_type = NATType::FULL_CONE,
        .port_strategy = PortStrategy::CONTINUOUS,
        .port_delta = 1,
        .udp_timeout = 600s,
        .tcp_timeout = 7200s,
        .alg_enabled = true,
        .hairpin_supported = true,
        .strict_inbound_filter = false,
        .heartbeat_interval = 120s,
        .punch_strategy = PunchStrategy::STANDARD,
        .notes = "Xiaomi consumer router, P2P friendly"
    };

    // Unknown - Default conservative profile
    profiles_[DeviceVendor::UNKNOWN] = {
        .vendor = DeviceVendor::UNKNOWN,
        .nat_type = NATType::SYMMETRIC,
        .port_strategy = PortStrategy::RANDOM,
        .port_delta = 0,
        .udp_timeout = 180s,
        .tcp_timeout = 3600s,
        .alg_enabled = false,
        .hairpin_supported = false,
        .strict_inbound_filter = true,
        .heartbeat_interval = 45s,
        .punch_strategy = PunchStrategy::RELAY,
        .notes = "Unknown device, use conservative settings"
    };
}

DetectionResult DeviceDetector::detect(
    NATType nat_type,
    PortStrategy port_strategy,
    int port_delta,
    bool alg_detected,
    bool hairpin_detected
) const {
    std::vector<std::pair<DeviceVendor, double>> candidates;

    for (const auto& [vendor, profile] : profiles_) {
        if (vendor == DeviceVendor::UNKNOWN) continue;

        double score = 0.0;

        // NAT type match (weight: 40%)
        if (profile.nat_type == nat_type) {
            score += 0.4;
        }

        // Port strategy match (weight: 30%)
        if (profile.port_strategy == port_strategy) {
            score += 0.3;
        }

        // Port delta match (weight: 10%)
        if (port_strategy == PortStrategy::CONTINUOUS &&
            std::abs(profile.port_delta - port_delta) <= 1) {
            score += 0.1;
        }

        // ALG detection match (weight: 10%)
        if (profile.alg_enabled == alg_detected) {
            score += 0.1;
        }

        // Hairpin detection match (weight: 10%)
        if (profile.hairpin_supported == hairpin_detected) {
            score += 0.1;
        }

        if (score > 0.0) {
            candidates.push_back({vendor, score});
        }
    }

    if (candidates.empty()) {
        return {DeviceVendor::UNKNOWN, 0.0, "No matching profile found"};
    }

    // Sort by score descending
    std::sort(candidates.begin(), candidates.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    auto [best_vendor, best_score] = candidates[0];

    std::string reason = "Matched: NAT=" + to_string(nat_type) +
                        ", Port=" + to_string(port_strategy);

    return {best_vendor, best_score, reason};
}

const DeviceProfile& DeviceDetector::get_profile(DeviceVendor vendor) const {
    auto it = profiles_.find(vendor);
    if (it == profiles_.end()) {
        return profiles_.at(DeviceVendor::UNKNOWN);
    }
    return it->second;
}

const std::map<DeviceVendor, DeviceProfile>& DeviceDetector::get_all_profiles() const {
    return profiles_;
}

} // namespace detection
} // namespace p2p
