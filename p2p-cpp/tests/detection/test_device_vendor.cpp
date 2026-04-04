#include "p2p/detection/device_vendor.hpp"
#include <gtest/gtest.h>

using namespace p2p::detection;

class DeviceVendorTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(DeviceVendorTest, DeviceVendorToString_AllVendors) {
    EXPECT_EQ(to_string(DeviceVendor::HUAWEI), "Huawei");
    EXPECT_EQ(to_string(DeviceVendor::ZTE), "ZTE");
    EXPECT_EQ(to_string(DeviceVendor::ERICSSON), "Ericsson");
    EXPECT_EQ(to_string(DeviceVendor::NOKIA), "Nokia");
    EXPECT_EQ(to_string(DeviceVendor::FIBERHOME), "FiberHome");
    EXPECT_EQ(to_string(DeviceVendor::ALCATEL_LUCENT), "Alcatel-Lucent");
    EXPECT_EQ(to_string(DeviceVendor::SAMSUNG), "Samsung");
    EXPECT_EQ(to_string(DeviceVendor::CISCO), "Cisco");
    EXPECT_EQ(to_string(DeviceVendor::H3C), "H3C");
    EXPECT_EQ(to_string(DeviceVendor::SANGFOR), "Sangfor");
    EXPECT_EQ(to_string(DeviceVendor::QIANXIN), "Qianxin");
    EXPECT_EQ(to_string(DeviceVendor::PALO_ALTO), "Palo Alto");
    EXPECT_EQ(to_string(DeviceVendor::FORTINET), "Fortinet");
    EXPECT_EQ(to_string(DeviceVendor::JUNIPER), "Juniper");
    EXPECT_EQ(to_string(DeviceVendor::CHECKPOINT), "Check Point");
    EXPECT_EQ(to_string(DeviceVendor::TP_LINK), "TP-Link");
    EXPECT_EQ(to_string(DeviceVendor::XIAOMI), "Xiaomi");
    EXPECT_EQ(to_string(DeviceVendor::UNKNOWN), "Unknown");
}

TEST_F(DeviceVendorTest, DeviceVendorFromString_AllVendors) {
    EXPECT_EQ(device_vendor_from_string("Huawei"), DeviceVendor::HUAWEI);
    EXPECT_EQ(device_vendor_from_string("ZTE"), DeviceVendor::ZTE);
    EXPECT_EQ(device_vendor_from_string("Ericsson"), DeviceVendor::ERICSSON);
    EXPECT_EQ(device_vendor_from_string("Nokia"), DeviceVendor::NOKIA);
    EXPECT_EQ(device_vendor_from_string("FiberHome"), DeviceVendor::FIBERHOME);
    EXPECT_EQ(device_vendor_from_string("Alcatel-Lucent"), DeviceVendor::ALCATEL_LUCENT);
    EXPECT_EQ(device_vendor_from_string("Samsung"), DeviceVendor::SAMSUNG);
    EXPECT_EQ(device_vendor_from_string("Cisco"), DeviceVendor::CISCO);
    EXPECT_EQ(device_vendor_from_string("H3C"), DeviceVendor::H3C);
    EXPECT_EQ(device_vendor_from_string("Sangfor"), DeviceVendor::SANGFOR);
    EXPECT_EQ(device_vendor_from_string("Qianxin"), DeviceVendor::QIANXIN);
    EXPECT_EQ(device_vendor_from_string("Palo Alto"), DeviceVendor::PALO_ALTO);
    EXPECT_EQ(device_vendor_from_string("Fortinet"), DeviceVendor::FORTINET);
    EXPECT_EQ(device_vendor_from_string("Juniper"), DeviceVendor::JUNIPER);
    EXPECT_EQ(device_vendor_from_string("Check Point"), DeviceVendor::CHECKPOINT);
    EXPECT_EQ(device_vendor_from_string("TP-Link"), DeviceVendor::TP_LINK);
    EXPECT_EQ(device_vendor_from_string("Xiaomi"), DeviceVendor::XIAOMI);
}

TEST_F(DeviceVendorTest, DeviceVendorFromString_UnknownVendor) {
    EXPECT_EQ(device_vendor_from_string("InvalidVendor"), DeviceVendor::UNKNOWN);
    EXPECT_EQ(device_vendor_from_string(""), DeviceVendor::UNKNOWN);
    EXPECT_EQ(device_vendor_from_string("huawei"), DeviceVendor::UNKNOWN);  // Case sensitive
}

TEST_F(DeviceVendorTest, DeviceVendorRoundTrip) {
    // Test that to_string and from_string are inverses
    auto vendors = {
        DeviceVendor::HUAWEI, DeviceVendor::ZTE, DeviceVendor::ERICSSON,
        DeviceVendor::NOKIA, DeviceVendor::FIBERHOME, DeviceVendor::ALCATEL_LUCENT,
        DeviceVendor::SAMSUNG, DeviceVendor::CISCO, DeviceVendor::H3C,
        DeviceVendor::SANGFOR, DeviceVendor::QIANXIN, DeviceVendor::PALO_ALTO,
        DeviceVendor::FORTINET, DeviceVendor::JUNIPER, DeviceVendor::CHECKPOINT,
        DeviceVendor::TP_LINK, DeviceVendor::XIAOMI
    };

    for (auto vendor : vendors) {
        std::string str = to_string(vendor);
        DeviceVendor parsed = device_vendor_from_string(str);
        EXPECT_EQ(parsed, vendor) << "Failed for vendor: " << str;
    }
}

TEST_F(DeviceVendorTest, NATTypeToString_AllTypes) {
    EXPECT_EQ(to_string(NATType::PUBLIC), "Public");
    EXPECT_EQ(to_string(NATType::FULL_CONE), "Full Cone");
    EXPECT_EQ(to_string(NATType::RESTRICTED_CONE), "Restricted Cone");
    EXPECT_EQ(to_string(NATType::PORT_RESTRICTED), "Port Restricted");
    EXPECT_EQ(to_string(NATType::SYMMETRIC), "Symmetric");
    EXPECT_EQ(to_string(NATType::UNKNOWN), "Unknown");
}

TEST_F(DeviceVendorTest, PortStrategyToString_AllStrategies) {
    EXPECT_EQ(to_string(PortStrategy::CONTINUOUS), "Continuous");
    EXPECT_EQ(to_string(PortStrategy::JUMP), "Jump");
    EXPECT_EQ(to_string(PortStrategy::RANDOM), "Random");
    EXPECT_EQ(to_string(PortStrategy::HYBRID), "Hybrid");
}

TEST_F(DeviceVendorTest, PunchStrategyToString_AllStrategies) {
    EXPECT_EQ(to_string(PunchStrategy::STANDARD), "Standard");
    EXPECT_EQ(to_string(PunchStrategy::MULTI_PORT), "Multi-Port");
    EXPECT_EQ(to_string(PunchStrategy::MULTI_PORT_RETRY), "Multi-Port Retry");
    EXPECT_EQ(to_string(PunchStrategy::TCP_FALLBACK), "TCP Fallback");
    EXPECT_EQ(to_string(PunchStrategy::RELAY), "Relay");
    EXPECT_EQ(to_string(PunchStrategy::RELAY_IMMEDIATELY), "Relay Immediately");
    EXPECT_EQ(to_string(PunchStrategy::CHECK_POLICY), "Check Policy");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
