#pragma once

#include "device_vendor.hpp"
#include <string>
#include <vector>
#include <map>
#include <optional>

namespace p2p {
namespace detection {

// Detection result
struct DetectionResult {
    DeviceVendor vendor;
    double confidence;  // 0.0 - 1.0
    std::string reason;
};

// Device detector class
class DeviceDetector {
public:
    DeviceDetector();

    // Detect device vendor based on NAT behavior
    DetectionResult detect(
        NATType nat_type,
        PortStrategy port_strategy,
        int port_delta,
        bool alg_detected,
        bool hairpin_detected
    ) const;

    // Get device profile by vendor
    const DeviceProfile& get_profile(DeviceVendor vendor) const;

    // Get all profiles
    const std::map<DeviceVendor, DeviceProfile>& get_all_profiles() const;

private:
    std::map<DeviceVendor, DeviceProfile> profiles_;

    void initialize_profiles();
};

} // namespace detection
} // namespace p2p
