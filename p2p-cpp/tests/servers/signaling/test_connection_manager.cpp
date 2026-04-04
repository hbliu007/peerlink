#include <gtest/gtest.h>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>

#include <chrono>
#include <future>
#include <memory>
#include <string>
#include <thread>

#include "connection_manager.hpp"
#include "websocket_session.hpp"

namespace signaling {
namespace {

TEST(ConnectionManagerTest, AddDevice) {
    auto manager = std::make_shared<ConnectionManager>();
    boost::asio::io_context io_context;

    auto future = boost::asio::co_spawn(
        io_context,
        manager->connect("device-1", nullptr, "pk-1", {"tunnel"}),
        boost::asio::use_future);
    io_context.run();
    future.get();

    EXPECT_TRUE(manager->is_connected("device-1"));
    EXPECT_EQ(manager->device_count(), 1);

    auto device = manager->get_device("device-1");
    ASSERT_TRUE(device.has_value());
    EXPECT_EQ(device->device_id, "device-1");
    EXPECT_EQ(device->public_key, "pk-1");
    EXPECT_EQ(device->capabilities.size(), 1);
    EXPECT_EQ(device->capabilities[0], "tunnel");
}

TEST(ConnectionManagerTest, RemoveDevice) {
    auto manager = std::make_shared<ConnectionManager>();
    boost::asio::io_context io_context;

    auto connect_future = boost::asio::co_spawn(
        io_context,
        manager->connect("device-1", nullptr, "pk-1", {}),
        boost::asio::use_future);
    io_context.run();
    connect_future.get();

    EXPECT_TRUE(manager->is_connected("device-1"));
    io_context.restart();

    auto disconnect_future = boost::asio::co_spawn(
        io_context,
        manager->disconnect("device-1"),
        boost::asio::use_future);
    io_context.run();
    disconnect_future.get();

    EXPECT_FALSE(manager->is_connected("device-1"));
    EXPECT_EQ(manager->device_count(), 0);
}

TEST(ConnectionManagerTest, ReplaceExistingDevice) {
    auto manager = std::make_shared<ConnectionManager>();
    boost::asio::io_context io_context;

    auto future1 = boost::asio::co_spawn(
        io_context,
        manager->connect("device-1", nullptr, "pk-old", {"tunnel"}),
        boost::asio::use_future);
    io_context.run();
    future1.get();
    io_context.restart();

    auto future2 = boost::asio::co_spawn(
        io_context,
        manager->connect("device-1", nullptr, "pk-new", {"relay"}),
        boost::asio::use_future);
    io_context.run();
    future2.get();

    EXPECT_EQ(manager->device_count(), 1);
    auto device = manager->get_device("device-1");
    ASSERT_TRUE(device.has_value());
    EXPECT_EQ(device->public_key, "pk-new");
    EXPECT_EQ(device->capabilities[0], "relay");
}

TEST(ConnectionManagerTest, GetAllDevices) {
    auto manager = std::make_shared<ConnectionManager>();
    boost::asio::io_context io_context;

    auto f1 = boost::asio::co_spawn(io_context,
        manager->connect("device-1", nullptr, "pk-1", {}),
        boost::asio::use_future);
    io_context.run();
    f1.get();
    io_context.restart();

    auto f2 = boost::asio::co_spawn(io_context,
        manager->connect("device-2", nullptr, "pk-2", {}),
        boost::asio::use_future);
    io_context.run();
    f2.get();

    auto devices = manager->get_all_devices();
    EXPECT_EQ(devices.size(), 2);
    EXPECT_TRUE(devices.count("device-1") > 0);
    EXPECT_TRUE(devices.count("device-2") > 0);
}

TEST(ConnectionManagerTest, CreateSession) {
    auto manager = std::make_shared<ConnectionManager>();

    auto session = manager->create_session("device-a", "device-b");

    EXPECT_FALSE(session.session_id.empty());
    EXPECT_EQ(session.device_a, "device-a");
    EXPECT_EQ(session.device_b, "device-b");
    EXPECT_EQ(session.status, ConnectionStatus::CONNECTING);
    EXPECT_FALSE(session.use_relay);
}

TEST(ConnectionManagerTest, GetSession) {
    auto manager = std::make_shared<ConnectionManager>();

    auto created = manager->create_session("device-a", "device-b");
    auto retrieved = manager->get_session(created.session_id);

    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->session_id, created.session_id);
    EXPECT_EQ(retrieved->device_a, "device-a");
    EXPECT_EQ(retrieved->device_b, "device-b");
}

TEST(ConnectionManagerTest, GetSessionByDevices) {
    auto manager = std::make_shared<ConnectionManager>();

    auto created = manager->create_session("device-a", "device-b");
    auto retrieved = manager->get_session_by_devices("device-a", "device-b");

    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->session_id, created.session_id);
}

TEST(ConnectionManagerTest, RemoveSession) {
    auto manager = std::make_shared<ConnectionManager>();

    auto session = manager->create_session("device-a", "device-b");
    EXPECT_TRUE(manager->get_session(session.session_id).has_value());

    bool removed = manager->remove_session(session.session_id);
    EXPECT_TRUE(removed);
    EXPECT_FALSE(manager->get_session(session.session_id).has_value());
}

TEST(ConnectionManagerTest, UpdateSessionStatus) {
    auto manager = std::make_shared<ConnectionManager>();

    auto session = manager->create_session("device-a", "device-b");
    manager->update_session_status(session.session_id, ConnectionStatus::CONNECTED);

    auto updated = manager->get_session(session.session_id);
    ASSERT_TRUE(updated.has_value());
    EXPECT_EQ(updated->status, ConnectionStatus::CONNECTED);
}

TEST(ConnectionManagerTest, SetSessionOfferAndAnswer) {
    auto manager = std::make_shared<ConnectionManager>();

    auto session = manager->create_session("device-a", "device-b");
    manager->set_session_offer(session.session_id, "offer-sdp");
    manager->set_session_answer(session.session_id, "answer-sdp");

    auto updated = manager->get_session(session.session_id);
    ASSERT_TRUE(updated.has_value());
    EXPECT_EQ(updated->offer, "offer-sdp");
    EXPECT_EQ(updated->answer, "answer-sdp");
}

TEST(ConnectionManagerTest, AddIceCandidate) {
    auto manager = std::make_shared<ConnectionManager>();

    auto session = manager->create_session("device-a", "device-b");
    json candidate = {{"candidate", "ice-candidate-1"}};
    manager->add_ice_candidate(session.session_id, "device-a", candidate);

    auto updated = manager->get_session(session.session_id);
    ASSERT_TRUE(updated.has_value());
    EXPECT_EQ(updated->ice_candidates_a.size(), 1);
    EXPECT_EQ(updated->ice_candidates_a[0]["candidate"], "ice-candidate-1");
}

TEST(ConnectionManagerTest, SetRelayMode) {
    auto manager = std::make_shared<ConnectionManager>();

    auto session = manager->create_session("device-a", "device-b");
    manager->set_relay_mode(session.session_id);

    auto updated = manager->get_session(session.session_id);
    ASSERT_TRUE(updated.has_value());
    EXPECT_TRUE(updated->use_relay);
}

TEST(ConnectionManagerTest, PublishAndGetService) {
    auto manager = std::make_shared<ConnectionManager>();

    PublishedService service;
    service.name = "test-service";
    service.owner_device_id = "device-1";
    service.allowed_callers = {"device-2"};

    manager->publish_service(service);
    EXPECT_EQ(manager->service_count(), 1);

    auto retrieved = manager->get_service("test-service");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->name, "test-service");
    EXPECT_EQ(retrieved->owner_device_id, "device-1");
}

TEST(ConnectionManagerTest, UnpublishService) {
    auto manager = std::make_shared<ConnectionManager>();

    PublishedService service;
    service.name = "test-service";
    service.owner_device_id = "device-1";
    manager->publish_service(service);

    bool removed = manager->unpublish_service("device-1", "test-service");
    EXPECT_TRUE(removed);
    EXPECT_EQ(manager->service_count(), 0);
}

TEST(ConnectionManagerTest, PendingRequest) {
    auto manager = std::make_shared<ConnectionManager>();

    manager->add_pending_request("session-1", "device-a");

    auto requester = manager->get_pending_requester("session-1");
    ASSERT_TRUE(requester.has_value());
    EXPECT_EQ(*requester, "device-a");

    manager->remove_pending_request("session-1");
    EXPECT_FALSE(manager->get_pending_requester("session-1").has_value());
}

TEST(ConnectionManagerTest, UpdateHeartbeat) {
    auto manager = std::make_shared<ConnectionManager>();
    boost::asio::io_context io_context;

    auto connect_future = boost::asio::co_spawn(
        io_context,
        manager->connect("device-1", nullptr, "pk-1", {}),
        boost::asio::use_future);
    io_context.run();
    connect_future.get();
    io_context.restart();

    auto heartbeat_future = boost::asio::co_spawn(
        io_context,
        manager->update_heartbeat("device-1"),
        boost::asio::use_future);
    io_context.run();
    bool updated = heartbeat_future.get();

    EXPECT_TRUE(updated);
}

TEST(ConnectionManagerTest, CleanupStale) {
    auto manager = std::make_shared<ConnectionManager>();
    boost::asio::io_context io_context;

    auto connect_future = boost::asio::co_spawn(
        io_context,
        manager->connect("device-1", nullptr, "pk-1", {}),
        boost::asio::use_future);
    io_context.run();
    connect_future.get();

    std::this_thread::sleep_for(std::chrono::seconds(2));
    io_context.restart();

    auto cleanup_future = boost::asio::co_spawn(
        io_context,
        manager->cleanup_stale(1),
        boost::asio::use_future);
    io_context.run();
    int cleaned = cleanup_future.get();

    EXPECT_EQ(cleaned, 1);
    EXPECT_FALSE(manager->is_connected("device-1"));
}

}  // namespace
}  // namespace signaling
