#include <gtest/gtest.h>
#include "p2p/detection/device_detector.hpp"

using namespace p2p::detection;

class DeviceDetectorTest : public ::testing::Test {
protected:
    DeviceDetector detector;
};

TEST_F(DeviceDetectorTest, DetectHuawei) {
    auto result = detector.detect(
        NATType::PORT_RESTRICTED,
        PortStrategy::CONTINUOUS,
        1,
        true,  // ALG enabled
        false  // No hairpin
    );

    EXPECT_EQ(result.vendor, DeviceVendor::HUAWEI);
    EXPECT_GT(result.confidence, 0.8);
}

TEST_F(DeviceDetectorTest, DetectEricssonSymmetric) {
    auto result = detector.detect(
        NATType::SYMMETRIC,
        PortStrategy::RANDOM,
        0,
        false,  // No ALG
        false   // No hairpin
    );

    EXPECT_TRUE(
        result.vendor == DeviceVendor::ERICSSON ||
        result.vendor == DeviceVendor::NOKIA ||
        result.vendor == DeviceVendor::SANGFOR ||
        result.vendor == DeviceVendor::QIANXIN ||
        result.vendor == DeviceVendor::ALCATEL_LUCENT
    );
    EXPECT_GT(result.confidence, 0.5);
}

TEST_F(DeviceDetectorTest, DetectTPLink) {
    auto result = detector.detect(
        NATType::FULL_CONE,
        PortStrategy::CONTINUOUS,
        1,
        true,  // ALG enabled
        true   // Hairpin supported
    );

    EXPECT_TRUE(
        result.vendor == DeviceVendor::TP_LINK ||
        result.vendor == DeviceVendor::XIAOMI
    );
    EXPECT_GT(result.confidence, 0.8);
}

TEST_F(DeviceDetectorTest, DetectCisco) {
    auto result = detector.detect(
        NATType::PORT_RESTRICTED,
        PortStrategy::CONTINUOUS,
        1,
        true,  // ALG enabled
        true   // Hairpin supported
    );

    // Should match enterprise devices with hairpin support
    EXPECT_TRUE(
        result.vendor == DeviceVendor::CISCO ||
        result.vendor == DeviceVendor::H3C ||
        result.vendor == DeviceVendor::PALO_ALTO ||
        result.vendor == DeviceVendor::FORTINET ||
        result.vendor == DeviceVendor::JUNIPER ||
        result.vendor == DeviceVendor::CHECKPOINT ||
        result.vendor == DeviceVendor::TP_LINK ||
        result.vendor == DeviceVendor::XIAOMI
    );
}

TEST_F(DeviceDetectorTest, GetProfileHuawei) {
    const auto& profile = detector.get_profile(DeviceVendor::HUAWEI);

    EXPECT_EQ(profile.vendor, DeviceVendor::HUAWEI);
    EXPECT_EQ(profile.nat_type, NATType::PORT_RESTRICTED);
    EXPECT_EQ(profile.port_strategy, PortStrategy::CONTINUOUS);
    EXPECT_EQ(profile.port_delta, 1);
    EXPECT_TRUE(profile.alg_enabled);
    EXPECT_FALSE(profile.hairpin_supported);
    EXPECT_TRUE(profile.strict_inbound_filter);
    EXPECT_EQ(profile.punch_strategy, PunchStrategy::MULTI_PORT_RETRY);
}

TEST_F(DeviceDetectorTest, GetProfileUnknown) {
    const auto& profile = detector.get_profile(DeviceVendor::UNKNOWN);

    EXPECT_EQ(profile.vendor, DeviceVendor::UNKNOWN);
    EXPECT_EQ(profile.nat_type, NATType::SYMMETRIC);
    EXPECT_EQ(profile.punch_strategy, PunchStrategy::RELAY);
}

TEST_F(DeviceDetectorTest, GetAllProfiles) {
    const auto& profiles = detector.get_all_profiles();

    // Should have all 18 vendors (17 known + 1 unknown)
    EXPECT_EQ(profiles.size(), 18);

    // Verify key vendors exist
    EXPECT_TRUE(profiles.find(DeviceVendor::HUAWEI) != profiles.end());
    EXPECT_TRUE(profiles.find(DeviceVendor::CISCO) != profiles.end());
    EXPECT_TRUE(profiles.find(DeviceVendor::TP_LINK) != profiles.end());
    EXPECT_TRUE(profiles.find(DeviceVendor::UNKNOWN) != profiles.end());
}

TEST_F(DeviceDetectorTest, ToStringConversions) {
    EXPECT_EQ(to_string(DeviceVendor::HUAWEI), "Huawei");
    EXPECT_EQ(to_string(DeviceVendor::CISCO), "Cisco");
    EXPECT_EQ(to_string(DeviceVendor::TP_LINK), "TP-Link");

    EXPECT_EQ(to_string(NATType::FULL_CONE), "Full Cone");
    EXPECT_EQ(to_string(NATType::SYMMETRIC), "Symmetric");

    EXPECT_EQ(to_string(PortStrategy::CONTINUOUS), "Continuous");
    EXPECT_EQ(to_string(PortStrategy::RANDOM), "Random");

    EXPECT_EQ(to_string(PunchStrategy::STANDARD), "Standard");
    EXPECT_EQ(to_string(PunchStrategy::RELAY), "Relay");
}

TEST_F(DeviceDetectorTest, FromStringConversions) {
    EXPECT_EQ(device_vendor_from_string("Huawei"), DeviceVendor::HUAWEI);
    EXPECT_EQ(device_vendor_from_string("Cisco"), DeviceVendor::CISCO);
    EXPECT_EQ(device_vendor_from_string("TP-Link"), DeviceVendor::TP_LINK);
    EXPECT_EQ(device_vendor_from_string("Invalid"), DeviceVendor::UNKNOWN);
}

TEST_F(DeviceDetectorTest, NoMatchReturnsUnknown) {
    // Use completely mismatched parameters that won't score well
    auto result = detector.detect(
        NATType::UNKNOWN,
        PortStrategy::JUMP,
        999,
        false,
        false
    );

    // Should return UNKNOWN with low confidence or no match
    EXPECT_TRUE(result.vendor == DeviceVendor::UNKNOWN || result.confidence < 0.3);
}
