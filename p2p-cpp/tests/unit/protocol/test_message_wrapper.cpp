#include <gtest/gtest.h>
#include "p2p/protocol/dcutr_message.hpp"
#include "p2p/protocol/relay_message.hpp"
#include <chrono>

using namespace p2p::protocol;

class MessageWrapperTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Sample multiaddr (simplified)
        test_addr1_ = {0x04, 0x7f, 0x00, 0x00, 0x01, 0x06, 0x1f, 0x90};  // /ip4/127.0.0.1/tcp/8080
        test_addr2_ = {0x04, 0xc0, 0xa8, 0x01, 0x01, 0x06, 0x1f, 0x91};  // /ip4/192.168.1.1/tcp/8081
        test_addrs_ = {test_addr1_, test_addr2_};

        test_peer_id_ = {0x12, 0x20, 0x01, 0x02, 0x03};  // Simplified peer ID
    }

    std::vector<uint8_t> test_addr1_;
    std::vector<uint8_t> test_addr2_;
    std::vector<std::vector<uint8_t>> test_addrs_;
    std::vector<uint8_t> test_peer_id_;
};

// DCUtR Message Tests
TEST_F(MessageWrapperTest, DCUtRConnectCreateAndSerialize) {
    int64_t timestamp = 1234567890123456789LL;

    auto msg = DCUtRMessageWrapper::CreateConnect(test_addrs_, timestamp);

    EXPECT_EQ(msg.GetType(), DCUtRMessageWrapper::Type::CONNECT);
    EXPECT_EQ(msg.GetConnectTimestamp(), timestamp);

    auto addrs = msg.GetConnectAddrs();
    ASSERT_EQ(addrs.size(), 2);
    EXPECT_EQ(addrs[0], test_addr1_);
    EXPECT_EQ(addrs[1], test_addr2_);

    // Serialize
    auto serialized = msg.Serialize();
    EXPECT_GT(serialized.size(), 0);
}

TEST_F(MessageWrapperTest, DCUtRConnectRoundTrip) {
    int64_t timestamp = 9876543210987654321LL;

    auto msg1 = DCUtRMessageWrapper::CreateConnect(test_addrs_, timestamp);
    auto serialized = msg1.Serialize();

    auto msg2 = DCUtRMessageWrapper::Deserialize(serialized);
    ASSERT_TRUE(msg2.has_value());

    EXPECT_EQ(msg2->GetType(), DCUtRMessageWrapper::Type::CONNECT);
    EXPECT_EQ(msg2->GetConnectTimestamp(), timestamp);

    auto addrs = msg2->GetConnectAddrs();
    ASSERT_EQ(addrs.size(), 2);
    EXPECT_EQ(addrs[0], test_addr1_);
}

TEST_F(MessageWrapperTest, DCUtRSyncCreateAndSerialize) {
    int64_t echo_timestamp = 1234567890123456789LL;
    int64_t timestamp = 9876543210987654321LL;

    auto msg = DCUtRMessageWrapper::CreateSync(test_addrs_, echo_timestamp, timestamp);

    EXPECT_EQ(msg.GetType(), DCUtRMessageWrapper::Type::SYNC);
    EXPECT_EQ(msg.GetSyncEchoTimestamp(), echo_timestamp);
    EXPECT_EQ(msg.GetSyncTimestamp(), timestamp);

    auto addrs = msg.GetSyncAddrs();
    ASSERT_EQ(addrs.size(), 2);
    EXPECT_EQ(addrs[0], test_addr1_);
    EXPECT_EQ(addrs[1], test_addr2_);

    // Serialize
    auto serialized = msg.Serialize();
    EXPECT_GT(serialized.size(), 0);
}

TEST_F(MessageWrapperTest, DCUtRSyncRoundTrip) {
    int64_t echo_timestamp = 1111111111111111111LL;
    int64_t timestamp = 2222222222222222222LL;

    auto msg1 = DCUtRMessageWrapper::CreateSync(test_addrs_, echo_timestamp, timestamp);
    auto serialized = msg1.Serialize();

    auto msg2 = DCUtRMessageWrapper::Deserialize(serialized);
    ASSERT_TRUE(msg2.has_value());

    EXPECT_EQ(msg2->GetType(), DCUtRMessageWrapper::Type::SYNC);
    EXPECT_EQ(msg2->GetSyncEchoTimestamp(), echo_timestamp);
    EXPECT_EQ(msg2->GetSyncTimestamp(), timestamp);

    auto addrs = msg2->GetSyncAddrs();
    ASSERT_EQ(addrs.size(), 2);
}

TEST_F(MessageWrapperTest, DCUtRInvalidDeserialization) {
    std::vector<uint8_t> invalid_data = {0xFF, 0xFF, 0xFF};

    auto msg = DCUtRMessageWrapper::Deserialize(invalid_data);
    EXPECT_FALSE(msg.has_value());
}

// Relay v2 Message Tests
TEST_F(MessageWrapperTest, RelayReserveCreateAndSerialize) {
    auto msg = RelayMessageWrapper::CreateReserve();

    EXPECT_EQ(msg.GetType(), RelayMessageType::RESERVE);

    // Serialize (may be empty for RESERVE with no data)
    auto serialized = msg.Serialize();
    EXPECT_NO_THROW(msg.Serialize());
}

TEST_F(MessageWrapperTest, RelayReserveRoundTrip) {
    auto msg1 = RelayMessageWrapper::CreateReserve();
    auto serialized = msg1.Serialize();

    auto msg2 = RelayMessageWrapper::Deserialize(serialized);
    ASSERT_TRUE(msg2.has_value());

    EXPECT_EQ(msg2->GetType(), RelayMessageType::RESERVE);
}

TEST_F(MessageWrapperTest, RelayConnectCreateAndSerialize) {
    PeerInfo peer;
    peer.id = test_peer_id_;
    peer.addrs = test_addrs_;

    auto msg = RelayMessageWrapper::CreateConnect(peer);

    EXPECT_EQ(msg.GetType(), RelayMessageType::CONNECT);

    auto peer_info = msg.GetPeer();
    ASSERT_TRUE(peer_info.has_value());
    EXPECT_EQ(peer_info->id, test_peer_id_);
    ASSERT_EQ(peer_info->addrs.size(), 2);
    EXPECT_EQ(peer_info->addrs[0], test_addr1_);

    // Serialize
    auto serialized = msg.Serialize();
    EXPECT_GT(serialized.size(), 0);
}

TEST_F(MessageWrapperTest, RelayConnectRoundTrip) {
    PeerInfo peer;
    peer.id = test_peer_id_;
    peer.addrs = test_addrs_;

    auto msg1 = RelayMessageWrapper::CreateConnect(peer);
    auto serialized = msg1.Serialize();

    auto msg2 = RelayMessageWrapper::Deserialize(serialized);
    ASSERT_TRUE(msg2.has_value());

    EXPECT_EQ(msg2->GetType(), RelayMessageType::CONNECT);

    auto peer_info = msg2->GetPeer();
    ASSERT_TRUE(peer_info.has_value());
    EXPECT_EQ(peer_info->id, test_peer_id_);
}

TEST_F(MessageWrapperTest, RelayStatusOKCreateAndSerialize) {
    auto msg = RelayMessageWrapper::CreateStatus(RelayStatusCode::OK, "Success");

    EXPECT_EQ(msg.GetType(), RelayMessageType::STATUS);
    EXPECT_EQ(msg.GetStatusCode(), RelayStatusCode::OK);
    EXPECT_EQ(msg.GetStatusText(), "Success");

    // Serialize
    auto serialized = msg.Serialize();
    EXPECT_GT(serialized.size(), 0);
}

TEST_F(MessageWrapperTest, RelayStatusWithReservation) {
    ReservationInfo reservation;
    reservation.expire = 1234567890;
    reservation.addr = test_addr1_;
    reservation.voucher = {0xAA, 0xBB, 0xCC};
    reservation.limit_duration = 3600;
    reservation.limit_data = 1000000;

    auto msg = RelayMessageWrapper::CreateStatus(
        RelayStatusCode::OK,
        "Reservation created",
        reservation);

    EXPECT_EQ(msg.GetType(), RelayMessageType::STATUS);
    EXPECT_EQ(msg.GetStatusCode(), RelayStatusCode::OK);

    auto res = msg.GetReservation();
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->expire, 1234567890);
    EXPECT_EQ(res->addr, test_addr1_);
    EXPECT_EQ(res->limit_duration, 3600);
    EXPECT_EQ(res->limit_data, 1000000);
}

TEST_F(MessageWrapperTest, RelayStatusRoundTrip) {
    auto msg1 = RelayMessageWrapper::CreateStatus(
        RelayStatusCode::PERMISSION_DENIED,
        "Access denied");

    auto serialized = msg1.Serialize();

    auto msg2 = RelayMessageWrapper::Deserialize(serialized);
    ASSERT_TRUE(msg2.has_value());

    EXPECT_EQ(msg2->GetType(), RelayMessageType::STATUS);
    EXPECT_EQ(msg2->GetStatusCode(), RelayStatusCode::PERMISSION_DENIED);
    EXPECT_EQ(msg2->GetStatusText(), "Access denied");
}

TEST_F(MessageWrapperTest, RelayInvalidDeserialization) {
    std::vector<uint8_t> invalid_data = {0xFF, 0xFF, 0xFF};

    auto msg = RelayMessageWrapper::Deserialize(invalid_data);
    EXPECT_FALSE(msg.has_value());
}

// Performance Tests
TEST_F(MessageWrapperTest, DCUtRSerializationPerformance) {
    int64_t timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    auto msg = DCUtRMessageWrapper::CreateConnect(test_addrs_, timestamp);

    const int num_iterations = 10000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_iterations; ++i) {
        auto serialized = msg.Serialize();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    double avg_time_ns = static_cast<double>(duration.count()) / num_iterations;

    std::cout << "Average DCUtR serialization time: " << avg_time_ns << " ns" << std::endl;

    // Should be fast (< 1000 ns = 1 μs per serialization)
    EXPECT_LT(avg_time_ns, 1000.0);
}

TEST_F(MessageWrapperTest, RelaySerializationPerformance) {
    PeerInfo peer;
    peer.id = test_peer_id_;
    peer.addrs = test_addrs_;

    auto msg = RelayMessageWrapper::CreateConnect(peer);

    const int num_iterations = 10000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_iterations; ++i) {
        auto serialized = msg.Serialize();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    double avg_time_ns = static_cast<double>(duration.count()) / num_iterations;

    std::cout << "Average Relay serialization time: " << avg_time_ns << " ns" << std::endl;

    // Should be fast (< 1000 ns = 1 μs per serialization)
    EXPECT_LT(avg_time_ns, 1000.0);
}
