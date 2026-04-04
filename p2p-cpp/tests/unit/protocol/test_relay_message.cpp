#include "p2p/protocol/relay_message.hpp"
#include <gtest/gtest.h>

using namespace p2p::protocol;

class RelayMessageWrapperTest : public ::testing::Test {
protected:
    // Helper: Create test PeerInfo with single address
    PeerInfo CreateTestPeerInfo() {
        PeerInfo peer;
        peer.id = {0x12, 0x20, 0xAB, 0xCD};  // 4-byte peer ID
        peer.addrs.push_back({0x04, 0x7F, 0x00, 0x00, 0x01});  // /ip4/127.0.0.1
        return peer;
    }

    // Helper: Create test PeerInfo with multiple addresses
    PeerInfo CreateMultiAddrPeerInfo() {
        PeerInfo peer;
        peer.id = {0x12, 0x20, 0xDE, 0xAD, 0xBE, 0xEF};
        peer.addrs.push_back({0x04, 0x7F, 0x00, 0x00, 0x01});  // addr1
        peer.addrs.push_back({0x04, 0xC0, 0xA8, 0x01, 0x01});  // addr2
        peer.addrs.push_back({0x04, 0x0A, 0x00, 0x00, 0x01});  // addr3
        return peer;
    }

    // Helper: Create test ReservationInfo
    ReservationInfo CreateTestReservationInfo() {
        ReservationInfo res;
        res.expire = 1234567890;
        res.addr = {0x04, 0x7F, 0x00, 0x00, 0x01, 0x06, 0x1F, 0x90};  // /ip4/127.0.0.1/tcp/8080
        res.voucher = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
        res.limit_duration = 3600;
        res.limit_data = 1048576;  // 1MB
        return res;
    }
};

// --- Factory Method Tests ---

TEST_F(RelayMessageWrapperTest, CreateReserve_SetsCorrectType) {
    auto msg = RelayMessageWrapper::CreateReserve();
    EXPECT_EQ(msg.GetType(), RelayMessageType::RESERVE);
}

TEST_F(RelayMessageWrapperTest, CreateConnect_WithSingleAddress) {
    auto peer = CreateTestPeerInfo();
    auto msg = RelayMessageWrapper::CreateConnect(peer);

    EXPECT_EQ(msg.GetType(), RelayMessageType::CONNECT);

    auto retrieved_peer = msg.GetPeer();
    ASSERT_TRUE(retrieved_peer.has_value());
    EXPECT_EQ(retrieved_peer->id, peer.id);
    ASSERT_EQ(retrieved_peer->addrs.size(), 1);
    EXPECT_EQ(retrieved_peer->addrs[0], peer.addrs[0]);
}

TEST_F(RelayMessageWrapperTest, CreateConnect_WithMultipleAddresses) {
    auto peer = CreateMultiAddrPeerInfo();
    auto msg = RelayMessageWrapper::CreateConnect(peer);

    auto retrieved_peer = msg.GetPeer();
    ASSERT_TRUE(retrieved_peer.has_value());
    EXPECT_EQ(retrieved_peer->id, peer.id);
    ASSERT_EQ(retrieved_peer->addrs.size(), 3);
    for (size_t i = 0; i < 3; ++i) {
        EXPECT_EQ(retrieved_peer->addrs[i], peer.addrs[i]);
    }
}

TEST_F(RelayMessageWrapperTest, CreateConnect_WithEmptyAddresses) {
    PeerInfo peer;
    peer.id = {0x12, 0x20, 0xFF};
    // peer.addrs is empty

    auto msg = RelayMessageWrapper::CreateConnect(peer);

    auto retrieved_peer = msg.GetPeer();
    ASSERT_TRUE(retrieved_peer.has_value());
    EXPECT_EQ(retrieved_peer->id, peer.id);
    EXPECT_TRUE(retrieved_peer->addrs.empty());
}

TEST_F(RelayMessageWrapperTest, CreateStatus_WithoutReservation) {
    auto msg = RelayMessageWrapper::CreateStatus(
        RelayStatusCode::OK,
        "Success");

    EXPECT_EQ(msg.GetType(), RelayMessageType::STATUS);
    EXPECT_EQ(msg.GetStatusCode(), RelayStatusCode::OK);
    EXPECT_EQ(msg.GetStatusText(), "Success");
    EXPECT_FALSE(msg.GetReservation().has_value());
}

TEST_F(RelayMessageWrapperTest, CreateStatus_WithReservation) {
    auto reservation = CreateTestReservationInfo();
    auto msg = RelayMessageWrapper::CreateStatus(
        RelayStatusCode::OK,
        "Reservation granted",
        reservation);

    EXPECT_EQ(msg.GetStatusCode(), RelayStatusCode::OK);
    EXPECT_EQ(msg.GetStatusText(), "Reservation granted");

    auto retrieved_res = msg.GetReservation();
    ASSERT_TRUE(retrieved_res.has_value());
    EXPECT_EQ(retrieved_res->expire, reservation.expire);
    EXPECT_EQ(retrieved_res->addr, reservation.addr);
    EXPECT_EQ(retrieved_res->voucher, reservation.voucher);
    EXPECT_EQ(retrieved_res->limit_duration, reservation.limit_duration);
    EXPECT_EQ(retrieved_res->limit_data, reservation.limit_data);
}

TEST_F(RelayMessageWrapperTest, CreateStatus_EmptyText) {
    auto msg = RelayMessageWrapper::CreateStatus(
        RelayStatusCode::PERMISSION_DENIED,
        "");

    EXPECT_EQ(msg.GetStatusCode(), RelayStatusCode::PERMISSION_DENIED);
    EXPECT_TRUE(msg.GetStatusText().empty());
}

TEST_F(RelayMessageWrapperTest, CreateStatus_AllStatusCodes) {
    // Test all status codes can be created and retrieved
    std::vector<RelayStatusCode> codes = {
        RelayStatusCode::OK,
        RelayStatusCode::RESERVATION_REFUSED,
        RelayStatusCode::RESOURCE_LIMIT_EXCEEDED,
        RelayStatusCode::PERMISSION_DENIED,
        RelayStatusCode::CONNECTION_FAILED,
        RelayStatusCode::NO_RESERVATION,
        RelayStatusCode::MALFORMED_MESSAGE,
        RelayStatusCode::UNEXPECTED_MESSAGE
    };

    for (auto code : codes) {
        auto msg = RelayMessageWrapper::CreateStatus(code, "test");
        EXPECT_EQ(msg.GetStatusCode(), code);
    }
}

// --- Serialize/Deserialize Roundtrip Tests ---

TEST_F(RelayMessageWrapperTest, SerializeDeserialize_Reserve_Roundtrip) {
    auto original = RelayMessageWrapper::CreateReserve();
    auto serialized = original.Serialize();

    // Note: RESERVE may serialize to empty if type enum value is 0 (protobuf default)
    // This is valid protobuf behavior - focus on roundtrip correctness

    auto deserialized = RelayMessageWrapper::Deserialize(serialized);
    ASSERT_TRUE(deserialized.has_value());
    EXPECT_EQ(deserialized->GetType(), RelayMessageType::RESERVE);
}

TEST_F(RelayMessageWrapperTest, SerializeDeserialize_Connect_Roundtrip) {
    auto peer = CreateMultiAddrPeerInfo();
    auto original = RelayMessageWrapper::CreateConnect(peer);
    auto serialized = original.Serialize();

    auto deserialized = RelayMessageWrapper::Deserialize(serialized);
    ASSERT_TRUE(deserialized.has_value());
    EXPECT_EQ(deserialized->GetType(), RelayMessageType::CONNECT);

    auto retrieved_peer = deserialized->GetPeer();
    ASSERT_TRUE(retrieved_peer.has_value());
    EXPECT_EQ(retrieved_peer->id, peer.id);
    EXPECT_EQ(retrieved_peer->addrs.size(), peer.addrs.size());
}

TEST_F(RelayMessageWrapperTest, SerializeDeserialize_Status_Roundtrip) {
    auto reservation = CreateTestReservationInfo();
    auto original = RelayMessageWrapper::CreateStatus(
        RelayStatusCode::OK,
        "Test message",
        reservation);
    auto serialized = original.Serialize();

    auto deserialized = RelayMessageWrapper::Deserialize(serialized);
    ASSERT_TRUE(deserialized.has_value());
    EXPECT_EQ(deserialized->GetType(), RelayMessageType::STATUS);
    EXPECT_EQ(deserialized->GetStatusCode(), RelayStatusCode::OK);
    EXPECT_EQ(deserialized->GetStatusText(), "Test message");

    auto retrieved_res = deserialized->GetReservation();
    ASSERT_TRUE(retrieved_res.has_value());
    EXPECT_EQ(retrieved_res->expire, reservation.expire);
    EXPECT_EQ(retrieved_res->voucher, reservation.voucher);
}

TEST_F(RelayMessageWrapperTest, Deserialize_InvalidData_ReturnsNullopt) {
    std::vector<uint8_t> invalid_data = {0xFF, 0xFE, 0xFD, 0xFC};
    auto result = RelayMessageWrapper::Deserialize(invalid_data);
    EXPECT_FALSE(result.has_value());
}

TEST_F(RelayMessageWrapperTest, Deserialize_EmptyData_CreatesDefaultMessage) {
    // Protobuf allows deserializing empty data - creates message with all default values
    std::vector<uint8_t> empty_data;
    auto result = RelayMessageWrapper::Deserialize(empty_data);

    ASSERT_TRUE(result.has_value());
    // Default message type is RESERVE (enum value 0)
    EXPECT_EQ(result->GetType(), RelayMessageType::RESERVE);
}

// --- Accessor Tests ---

TEST_F(RelayMessageWrapperTest, GetPeer_OnNonConnectMessage_ReturnsNullopt) {
    auto reserve_msg = RelayMessageWrapper::CreateReserve();
    EXPECT_FALSE(reserve_msg.GetPeer().has_value());

    auto status_msg = RelayMessageWrapper::CreateStatus(RelayStatusCode::OK, "");
    EXPECT_FALSE(status_msg.GetPeer().has_value());
}

TEST_F(RelayMessageWrapperTest, GetStatusCode_OnNonStatusMessage_ThrowsException) {
    auto reserve_msg = RelayMessageWrapper::CreateReserve();
    EXPECT_THROW(reserve_msg.GetStatusCode(), std::runtime_error);

    auto peer = CreateTestPeerInfo();
    auto connect_msg = RelayMessageWrapper::CreateConnect(peer);
    EXPECT_THROW(connect_msg.GetStatusCode(), std::runtime_error);
}

TEST_F(RelayMessageWrapperTest, GetStatusText_OnNonStatusMessage_ThrowsException) {
    auto reserve_msg = RelayMessageWrapper::CreateReserve();
    EXPECT_THROW(reserve_msg.GetStatusText(), std::runtime_error);

    auto peer = CreateTestPeerInfo();
    auto connect_msg = RelayMessageWrapper::CreateConnect(peer);
    EXPECT_THROW(connect_msg.GetStatusText(), std::runtime_error);
}

TEST_F(RelayMessageWrapperTest, GetReservation_OnNonStatusMessage_ReturnsNullopt) {
    auto reserve_msg = RelayMessageWrapper::CreateReserve();
    EXPECT_FALSE(reserve_msg.GetReservation().has_value());

    auto peer = CreateTestPeerInfo();
    auto connect_msg = RelayMessageWrapper::CreateConnect(peer);
    EXPECT_FALSE(connect_msg.GetReservation().has_value());
}

// --- Move Semantics Tests ---

TEST_F(RelayMessageWrapperTest, MoveConstructor_TransfersOwnership) {
    auto original = RelayMessageWrapper::CreateReserve();
    auto moved = std::move(original);

    EXPECT_EQ(moved.GetType(), RelayMessageType::RESERVE);
    // original is now in moved-from state, should not be used
}

TEST_F(RelayMessageWrapperTest, MoveAssignment_TransfersOwnership) {
    auto original = RelayMessageWrapper::CreateReserve();
    auto target = RelayMessageWrapper::CreateStatus(RelayStatusCode::OK, "");

    target = std::move(original);

    EXPECT_EQ(target.GetType(), RelayMessageType::RESERVE);
}

// --- Boundary Condition Tests ---

TEST_F(RelayMessageWrapperTest, CreateConnect_WithLargePeerId) {
    PeerInfo peer;
    // Create a large peer ID (256 bytes)
    peer.id.resize(256, 0xAB);
    peer.addrs.push_back({0x04, 0x7F, 0x00, 0x00, 0x01});

    auto msg = RelayMessageWrapper::CreateConnect(peer);
    auto retrieved_peer = msg.GetPeer();

    ASSERT_TRUE(retrieved_peer.has_value());
    EXPECT_EQ(retrieved_peer->id.size(), 256);
    EXPECT_EQ(retrieved_peer->id, peer.id);
}

TEST_F(RelayMessageWrapperTest, CreateStatus_WithLargeVoucher) {
    ReservationInfo res;
    res.expire = 9999999999;
    res.addr = {0x04, 0x7F, 0x00, 0x00, 0x01};
    // Create a large voucher (4KB)
    res.voucher.resize(4096, 0xCC);
    res.limit_duration = 7200;
    res.limit_data = 10485760;  // 10MB

    auto msg = RelayMessageWrapper::CreateStatus(
        RelayStatusCode::OK,
        "Large voucher test",
        res);

    auto retrieved_res = msg.GetReservation();
    ASSERT_TRUE(retrieved_res.has_value());
    EXPECT_EQ(retrieved_res->voucher.size(), 4096);
    EXPECT_EQ(retrieved_res->voucher, res.voucher);
}
