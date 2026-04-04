#include <gtest/gtest.h>
#include "p2p/protocol/negotiator.hpp"

using namespace p2p::protocol;

class NegotiatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        negotiator = std::make_unique<ProtocolNegotiator>();
    }

    std::unique_ptr<ProtocolNegotiator> negotiator;
};

// ProtocolVersion Tests
TEST_F(NegotiatorTest, ProtocolVersionToString) {
    ProtocolVersion version("/libp2p/dcutr", 1, 0, 0);
    EXPECT_EQ(version.ToString(), "/libp2p/dcutr/1.0.0");
}

TEST_F(NegotiatorTest, ProtocolVersionCompatibility) {
    ProtocolVersion v1_0_0("/libp2p/dcutr", 1, 0, 0);
    ProtocolVersion v1_1_0("/libp2p/dcutr", 1, 1, 0);
    ProtocolVersion v2_0_0("/libp2p/dcutr", 2, 0, 0);

    // Same major version is compatible
    EXPECT_TRUE(v1_0_0.IsCompatibleWith(v1_1_0));
    EXPECT_TRUE(v1_1_0.IsCompatibleWith(v1_0_0));

    // Different major version is not compatible
    EXPECT_FALSE(v1_0_0.IsCompatibleWith(v2_0_0));
    EXPECT_FALSE(v2_0_0.IsCompatibleWith(v1_0_0));
}

TEST_F(NegotiatorTest, ProtocolVersionEquality) {
    ProtocolVersion v1("/libp2p/dcutr", 1, 0, 0);
    ProtocolVersion v2("/libp2p/dcutr", 1, 0, 0);
    ProtocolVersion v3("/libp2p/dcutr", 1, 1, 0);

    EXPECT_TRUE(v1 == v2);
    EXPECT_FALSE(v1 == v3);
}

TEST_F(NegotiatorTest, ProtocolVersionComparison) {
    ProtocolVersion v1_0_0("/libp2p/dcutr", 1, 0, 0);
    ProtocolVersion v1_1_0("/libp2p/dcutr", 1, 1, 0);
    ProtocolVersion v2_0_0("/libp2p/dcutr", 2, 0, 0);

    EXPECT_TRUE(v1_0_0 < v1_1_0);
    EXPECT_TRUE(v1_1_0 < v2_0_0);
    EXPECT_FALSE(v2_0_0 < v1_0_0);
}

// ProtocolNegotiator Tests
TEST_F(NegotiatorTest, RegisterProtocol) {
    ProtocolVersion version("/libp2p/dcutr", 1, 0, 0);
    negotiator->RegisterProtocol(version);

    EXPECT_TRUE(negotiator->IsProtocolSupported("/libp2p/dcutr"));
}

TEST_F(NegotiatorTest, RegisterMultipleVersions) {
    std::vector<ProtocolVersion> versions = {
        ProtocolVersion("/libp2p/dcutr", 1, 0, 0),
        ProtocolVersion("/libp2p/dcutr", 1, 1, 0),
        ProtocolVersion("/libp2p/dcutr", 2, 0, 0)
    };

    negotiator->RegisterProtocols(versions);

    auto supported = negotiator->GetSupportedVersions("/libp2p/dcutr");
    EXPECT_EQ(supported.size(), 3);
}

TEST_F(NegotiatorTest, NegotiateExactMatch) {
    ProtocolVersion local_version("/libp2p/dcutr", 1, 0, 0);
    negotiator->RegisterProtocol(local_version);

    std::vector<ProtocolVersion> peer_versions = {
        ProtocolVersion("/libp2p/dcutr", 1, 0, 0)
    };

    auto response = negotiator->Negotiate(peer_versions);

    EXPECT_TRUE(response.IsSuccess());
    ASSERT_TRUE(response.negotiated_version.has_value());
    EXPECT_EQ(*response.negotiated_version, local_version);
}

TEST_F(NegotiatorTest, NegotiateCompatibleVersion) {
    ProtocolVersion local_version("/libp2p/dcutr", 1, 1, 0);
    negotiator->RegisterProtocol(local_version);

    std::vector<ProtocolVersion> peer_versions = {
        ProtocolVersion("/libp2p/dcutr", 1, 0, 0)
    };

    auto response = negotiator->Negotiate(peer_versions);

    EXPECT_TRUE(response.IsSuccess());
    ASSERT_TRUE(response.negotiated_version.has_value());
}

TEST_F(NegotiatorTest, NegotiateIncompatibleVersion) {
    ProtocolVersion local_version("/libp2p/dcutr", 1, 0, 0);
    negotiator->RegisterProtocol(local_version);

    std::vector<ProtocolVersion> peer_versions = {
        ProtocolVersion("/libp2p/dcutr", 2, 0, 0)
    };

    auto response = negotiator->Negotiate(peer_versions);

    EXPECT_FALSE(response.IsSuccess());
    EXPECT_EQ(response.result, NegotiationResult::VERSION_MISMATCH);
}

TEST_F(NegotiatorTest, NegotiateUnsupportedProtocol) {
    ProtocolVersion local_version("/libp2p/dcutr", 1, 0, 0);
    negotiator->RegisterProtocol(local_version);

    std::vector<ProtocolVersion> peer_versions = {
        ProtocolVersion("/libp2p/other", 1, 0, 0)
    };

    auto response = negotiator->Negotiate(peer_versions);

    EXPECT_FALSE(response.IsSuccess());
}

TEST_F(NegotiatorTest, NegotiateEmptyPeerVersions) {
    ProtocolVersion local_version("/libp2p/dcutr", 1, 0, 0);
    negotiator->RegisterProtocol(local_version);

    std::vector<ProtocolVersion> peer_versions;

    auto response = negotiator->Negotiate(peer_versions);

    EXPECT_FALSE(response.IsSuccess());
    EXPECT_EQ(response.result, NegotiationResult::INVALID_VERSION);
}

TEST_F(NegotiatorTest, NegotiateMultipleProtocols) {
    negotiator->RegisterProtocol(ProtocolVersion("/libp2p/dcutr", 1, 0, 0));
    negotiator->RegisterProtocol(ProtocolVersion("/libp2p/relay", 2, 0, 0));

    std::vector<ProtocolVersion> peer_versions = {
        ProtocolVersion("/libp2p/dcutr", 1, 0, 0),
        ProtocolVersion("/libp2p/relay", 2, 0, 0)
    };

    auto response = negotiator->Negotiate(peer_versions);

    EXPECT_TRUE(response.IsSuccess());
}

TEST_F(NegotiatorTest, BackwardCompatibility) {
    negotiator->EnableBackwardCompatibility(true);
    negotiator->RegisterProtocol(ProtocolVersion("/libp2p/dcutr", 1, 1, 0));

    std::vector<ProtocolVersion> peer_versions = {
        ProtocolVersion("/libp2p/dcutr", 1, 0, 0)
    };

    auto response = negotiator->Negotiate(peer_versions);

    EXPECT_TRUE(response.IsSuccess());
}

TEST_F(NegotiatorTest, StrictMode) {
    negotiator->EnableBackwardCompatibility(false);
    negotiator->RegisterProtocol(ProtocolVersion("/libp2p/dcutr", 1, 1, 0));

    std::vector<ProtocolVersion> peer_versions = {
        ProtocolVersion("/libp2p/dcutr", 1, 0, 0)
    };

    auto response = negotiator->Negotiate(peer_versions);

    EXPECT_FALSE(response.IsSuccess());
}

TEST_F(NegotiatorTest, ParseVersionWithNumbers) {
    auto version = ProtocolNegotiator::ParseVersion("/libp2p/circuit/relay/0.2.0/hop");

    ASSERT_TRUE(version.has_value());
    EXPECT_EQ(version->protocol_id, "/libp2p/circuit/relay/0.2.0/hop");
    EXPECT_EQ(version->major, 0);
    EXPECT_EQ(version->minor, 2);
    EXPECT_EQ(version->patch, 0);
}

TEST_F(NegotiatorTest, ParseVersionWithoutNumbers) {
    auto version = ProtocolNegotiator::ParseVersion("/libp2p/dcutr");

    ASSERT_TRUE(version.has_value());
    EXPECT_EQ(version->protocol_id, "/libp2p/dcutr");
    EXPECT_EQ(version->major, 1);
    EXPECT_EQ(version->minor, 0);
    EXPECT_EQ(version->patch, 0);
}

TEST_F(NegotiatorTest, GetAllSupportedProtocols) {
    negotiator->RegisterProtocol(ProtocolVersion("/libp2p/dcutr", 1, 0, 0));
    negotiator->RegisterProtocol(ProtocolVersion("/libp2p/relay", 2, 0, 0));

    auto all_protocols = negotiator->GetAllSupportedProtocols();

    EXPECT_EQ(all_protocols.size(), 2);
}

TEST_F(NegotiatorTest, CommonVersions) {
    // Test that common versions are defined
    EXPECT_EQ(CommonVersions::DCUTR_V1.major, 1);
    EXPECT_EQ(CommonVersions::RELAY_V2_HOP.major, 0);
    EXPECT_EQ(CommonVersions::RELAY_V2_HOP.minor, 2);
}

// Performance Tests
TEST_F(NegotiatorTest, NegotiationPerformance) {
    // Register multiple versions
    for (int i = 0; i < 10; i++) {
        negotiator->RegisterProtocol(ProtocolVersion("/libp2p/dcutr", 1, i, 0));
    }

    std::vector<ProtocolVersion> peer_versions;
    for (int i = 0; i < 10; i++) {
        peer_versions.push_back(ProtocolVersion("/libp2p/dcutr", 1, i, 0));
    }

    const int num_negotiations = 10000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_negotiations; i++) {
        negotiator->Negotiate(peer_versions);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    double avg_time_ns = static_cast<double>(duration.count()) / num_negotiations;

    std::cout << "Average negotiation time: " << avg_time_ns << " ns" << std::endl;

    // Should be fast (< 10000 ns = 10 μs per negotiation)
    EXPECT_LT(avg_time_ns, 10000.0);
}
