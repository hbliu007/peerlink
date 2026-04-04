#include <gtest/gtest.h>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>

#include <chrono>
#include <cstdlib>
#include <future>
#include <optional>
#include <string>

#include "servers/did/did_auth.hpp"
#include "connection_manager.hpp"
#include "message_handler.hpp"
#include "websocket_session.hpp"

namespace signaling {
namespace {

class ScopedEnvVar {
public:
    ScopedEnvVar(const char* name, const char* value) : name_(name) {
        const char* old_value = std::getenv(name_);
        if (old_value != nullptr) {
            old_value_ = old_value;
        }

        if (value != nullptr) {
            setenv(name_, value, 1);
        } else {
            unsetenv(name_);
        }
    }

    ~ScopedEnvVar() {
        if (old_value_.has_value()) {
            setenv(name_, old_value_->c_str(), 1);
        } else {
            unsetenv(name_);
        }
    }

private:
    const char* name_;
    std::optional<std::string> old_value_;
};

TEST(MessageHandlerTest, RelayRequestUsesConfiguredRelayEndpoint) {
    ScopedEnvVar relay_host("RELAY_PUBLIC_HOST", "198.51.100.10");
    ScopedEnvVar relay_port("RELAY_PUBLIC_PORT", "9001");
    json relay_info = BuildRelayInfoFromEnvironment();
    EXPECT_EQ(relay_info, json({
        {"host", "198.51.100.10"},
        {"port", 9001}
    }));
}

TEST(MessageHandlerTest, RelayRequestFallsBackToTurnEnvironmentVariables) {
    ScopedEnvVar relay_host("RELAY_PUBLIC_HOST", nullptr);
    ScopedEnvVar relay_port("RELAY_PUBLIC_PORT", nullptr);
    ScopedEnvVar turn_host("TURN_PUBLIC_IP", "203.0.113.50");
    ScopedEnvVar turn_port("TURN_PORT", "19001");
    json relay_info = BuildRelayInfoFromEnvironment();
    EXPECT_EQ(relay_info["host"], "203.0.113.50");
    EXPECT_EQ(relay_info["port"], 19001);
}

TEST(MessageHandlerTest, RelayHostOverrideStillFallsBackToTurnPort) {
    ScopedEnvVar relay_host("RELAY_PUBLIC_HOST", "relay.internal.example");
    ScopedEnvVar relay_port("RELAY_PUBLIC_PORT", nullptr);
    ScopedEnvVar turn_host("TURN_PUBLIC_IP", "203.0.113.50");
    ScopedEnvVar turn_port("TURN_PORT", "29001");
    json relay_info = BuildRelayInfoFromEnvironment();
    EXPECT_EQ(relay_info["host"], "relay.internal.example");
    EXPECT_EQ(relay_info["port"], 29001);
}

TEST(MessageHandlerTest, RelayRequestFailsWhenRelayNotificationCannotBeDelivered) {
    ScopedEnvVar allow_insecure("SIGNALING_ALLOW_INSECURE_REGISTRATION", "true");
    auto manager = std::make_shared<ConnectionManager>();
    boost::asio::io_context io_context;
    auto connect_future = boost::asio::co_spawn(
        io_context,
        manager->connect("peer-a", std::shared_ptr<WebSocketSession>{}, "", {}),
        boost::asio::use_future);
    io_context.run();
    connect_future.get();
    io_context.restart();
    ConnectionSession session = manager->create_session("peer-a", "peer-b");

    Message message{
        MessageType::RELAY_REQUEST,
        json{{"session_id", session.session_id}},
        std::chrono::system_clock::now(),
        std::string("peer-a"),
        std::nullopt,
        std::string("req-relay-fail")
    };

    MessageHandler handler(manager);
    std::future<std::optional<json>> future = boost::asio::co_spawn(
        io_context,
        handler.handle_message("peer-a", message),
        boost::asio::use_future
    );
    io_context.run();

    std::optional<json> response = future.get();
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ((*response)["type"], "error");
    EXPECT_EQ((*response)["data"]["code"], "CONNECTION_FAILED");

    auto updated_session = manager->get_session(session.session_id);
    ASSERT_TRUE(updated_session.has_value());
    EXPECT_FALSE(updated_session->use_relay);
}

TEST(MessageHandlerTest, RegisterUsesAuthTokenSubjectAsDeviceId) {
    ScopedEnvVar jwt_secret("JWT_SECRET", "unit-test-secret");
    ScopedEnvVar allow_insecure("SIGNALING_ALLOW_INSECURE_REGISTRATION", "false");

    auto manager = std::make_shared<ConnectionManager>();
    boost::asio::io_context io_context;
    auto seed_future = boost::asio::co_spawn(
        io_context,
        manager->connect("temp-client", std::shared_ptr<WebSocketSession>{}, "", {}),
        boost::asio::use_future);
    io_context.run();
    seed_future.get();
    io_context.restart();

    p2p::did::DidAuth auth("unit-test-secret");
    Message message{
        MessageType::REGISTER,
        json{
            {"auth_token", auth.GenerateToken("did:peerlink:test-device")},
            {"public_key", "pk-1"},
            {"capabilities", json::array({"tunnel", "relay"})}
        },
        std::chrono::system_clock::now(),
        std::string("temp-client"),
        std::nullopt,
        std::string("req-register")
    };

    MessageHandler handler(manager);
    std::future<std::optional<json>> future = boost::asio::co_spawn(
        io_context,
        handler.handle_message("temp-client", message),
        boost::asio::use_future);
    io_context.run();

    auto response = future.get();
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ((*response)["type"], "registered");
    EXPECT_EQ((*response)["data"]["device_id"], "did:peerlink:test-device");
    EXPECT_TRUE(manager->is_connected("did:peerlink:test-device"));
}

TEST(MessageHandlerTest, RegisterRejectsMissingTokenWhenSecureModeEnabled) {
    ScopedEnvVar jwt_secret("JWT_SECRET", "unit-test-secret");
    ScopedEnvVar allow_insecure("SIGNALING_ALLOW_INSECURE_REGISTRATION", "false");

    auto manager = std::make_shared<ConnectionManager>();
    boost::asio::io_context io_context;
    auto seed_future = boost::asio::co_spawn(
        io_context,
        manager->connect("temp-client", std::shared_ptr<WebSocketSession>{}, "", {}),
        boost::asio::use_future);
    io_context.run();
    seed_future.get();
    io_context.restart();

    Message message{
        MessageType::REGISTER,
        json{{"device_id", "plain-device"}},
        std::chrono::system_clock::now(),
        std::string("temp-client"),
        std::nullopt,
        std::string("req-register")
    };

    MessageHandler handler(manager);
    std::future<std::optional<json>> future = boost::asio::co_spawn(
        io_context,
        handler.handle_message("temp-client", message),
        boost::asio::use_future);
    io_context.run();

    auto response = future.get();
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ((*response)["type"], "error");
    EXPECT_EQ((*response)["data"]["code"], "UNAUTHORIZED");
}

TEST(MessageHandlerTest, RegisterRejectsNonStringCapabilities) {
    ScopedEnvVar jwt_secret("JWT_SECRET", "unit-test-secret");
    ScopedEnvVar allow_insecure("SIGNALING_ALLOW_INSECURE_REGISTRATION", "true");

    auto manager = std::make_shared<ConnectionManager>();
    boost::asio::io_context io_context;
    auto seed_future = boost::asio::co_spawn(
        io_context,
        manager->connect("temp-client", std::shared_ptr<WebSocketSession>{}, "", {}),
        boost::asio::use_future);
    io_context.run();
    seed_future.get();
    io_context.restart();

    Message message{
        MessageType::REGISTER,
        json{{"capabilities", 1}},
        std::chrono::system_clock::now(),
        std::string("temp-client"),
        std::nullopt,
        std::string("req-register-invalid-capabilities")
    };

    MessageHandler handler(manager);
    auto future = boost::asio::co_spawn(
        io_context,
        handler.handle_message("temp-client", message),
        boost::asio::use_future);
    io_context.run();

    auto response = future.get();
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ((*response)["type"], "error");
    EXPECT_EQ((*response)["data"]["code"], "INVALID_REQUEST");
}

TEST(MessageHandlerTest, ServicePublishAndQueryReturnPublishedService) {
    ScopedEnvVar allow_insecure("SIGNALING_ALLOW_INSECURE_REGISTRATION", "true");
    auto manager = std::make_shared<ConnectionManager>();
    boost::asio::io_context io_context;
    auto seed_future = boost::asio::co_spawn(
        io_context,
        manager->connect("device-a", std::shared_ptr<WebSocketSession>{}, "", {}),
        boost::asio::use_future);
    io_context.run();
    seed_future.get();
    io_context.restart();
    auto peer_b_future = boost::asio::co_spawn(
        io_context,
        manager->connect("device-b", std::shared_ptr<WebSocketSession>{}, "", {}),
        boost::asio::use_future);
    io_context.run();
    peer_b_future.get();
    io_context.restart();

    MessageHandler handler(manager);
    Message publish{
        MessageType::SERVICE_PUBLISH,
        json{
            {"service_name", "ssh-office"},
            {"relay_only", true},
            {"capabilities", json::array({"tcp", "relay"})},
            {"allowed_callers", json::array({"device-b"})},
            {"metadata", json{{"target_port", 22}}}
        },
        std::chrono::system_clock::now(),
        std::string("device-a"),
        std::nullopt,
        std::string("req-publish")
    };

    auto publish_future = boost::asio::co_spawn(
        io_context,
        handler.handle_message("device-a", publish),
        boost::asio::use_future);
    io_context.run();
    auto publish_response = publish_future.get();
    ASSERT_TRUE(publish_response.has_value());
    EXPECT_EQ((*publish_response)["type"], "service_published");
    io_context.restart();

    Message query{
        MessageType::SERVICE_QUERY,
        json{{"service_name", "ssh-office"}},
        std::chrono::system_clock::now(),
        std::string("device-b"),
        std::nullopt,
        std::string("req-query")
    };
    auto query_future = boost::asio::co_spawn(
        io_context,
        handler.handle_message("device-b", query),
        boost::asio::use_future);
    io_context.run();

    auto query_response = query_future.get();
    ASSERT_TRUE(query_response.has_value());
    EXPECT_EQ((*query_response)["type"], "service_info");
    EXPECT_EQ((*query_response)["data"]["service"]["name"], "ssh-office");
    EXPECT_TRUE((*query_response)["data"]["online"]);
    EXPECT_EQ((*query_response)["data"]["service"]["allowed_callers"], json::array({"device-b"}));
}

TEST(MessageHandlerTest, ServiceQueryRejectsUnauthorizedCaller) {
    ScopedEnvVar allow_insecure("SIGNALING_ALLOW_INSECURE_REGISTRATION", "true");
    auto manager = std::make_shared<ConnectionManager>();
    boost::asio::io_context io_context;
    auto device_a_future = boost::asio::co_spawn(
        io_context,
        manager->connect("device-a", std::shared_ptr<WebSocketSession>{}, "", {}),
        boost::asio::use_future);
    io_context.run();
    device_a_future.get();
    io_context.restart();
    auto device_b_future = boost::asio::co_spawn(
        io_context,
        manager->connect("device-b", std::shared_ptr<WebSocketSession>{}, "", {}),
        boost::asio::use_future);
    io_context.run();
    device_b_future.get();
    io_context.restart();
    auto device_c_future = boost::asio::co_spawn(
        io_context,
        manager->connect("device-c", std::shared_ptr<WebSocketSession>{}, "", {}),
        boost::asio::use_future);
    io_context.run();
    device_c_future.get();
    io_context.restart();

    MessageHandler handler(manager);
    Message publish{
        MessageType::SERVICE_PUBLISH,
        json{
            {"service_name", "private-ssh"},
            {"allowed_callers", json::array({"device-b"})}
        },
        std::chrono::system_clock::now(),
        std::string("device-a"),
        std::nullopt,
        std::string("req-publish-private")
    };
    auto publish_future = boost::asio::co_spawn(
        io_context,
        handler.handle_message("device-a", publish),
        boost::asio::use_future);
    io_context.run();
    ASSERT_TRUE(publish_future.get().has_value());
    io_context.restart();

    Message query{
        MessageType::SERVICE_QUERY,
        json{{"service_name", "private-ssh"}},
        std::chrono::system_clock::now(),
        std::string("device-c"),
        std::nullopt,
        std::string("req-query-unauthorized")
    };
    auto query_future = boost::asio::co_spawn(
        io_context,
        handler.handle_message("device-c", query),
        boost::asio::use_future);
    io_context.run();

    auto query_response = query_future.get();
    ASSERT_TRUE(query_response.has_value());
    EXPECT_EQ((*query_response)["type"], "service_info");
    EXPECT_EQ((*query_response)["data"]["service_name"], "private-ssh");
    EXPECT_FALSE((*query_response)["data"]["online"]);
}

TEST(MessageHandlerTest, ServicePublishRejectsNameTakeover) {
    ScopedEnvVar allow_insecure("SIGNALING_ALLOW_INSECURE_REGISTRATION", "true");
    auto manager = std::make_shared<ConnectionManager>();
    boost::asio::io_context io_context;
    auto device_a_future = boost::asio::co_spawn(
        io_context,
        manager->connect("device-a", std::shared_ptr<WebSocketSession>{}, "", {}),
        boost::asio::use_future);
    io_context.run();
    device_a_future.get();
    io_context.restart();
    auto device_b_future = boost::asio::co_spawn(
        io_context,
        manager->connect("device-b", std::shared_ptr<WebSocketSession>{}, "", {}),
        boost::asio::use_future);
    io_context.run();
    device_b_future.get();
    io_context.restart();

    MessageHandler handler(manager);
    Message first_publish{
        MessageType::SERVICE_PUBLISH,
        json{{"service_name", "ssh-office"}},
        std::chrono::system_clock::now(),
        std::string("device-a"),
        std::nullopt,
        std::string("req-publish-owner")
    };
    auto first_publish_future = boost::asio::co_spawn(
        io_context,
        handler.handle_message("device-a", first_publish),
        boost::asio::use_future);
    io_context.run();
    ASSERT_TRUE(first_publish_future.get().has_value());
    io_context.restart();

    Message takeover_publish{
        MessageType::SERVICE_PUBLISH,
        json{{"service_name", "ssh-office"}},
        std::chrono::system_clock::now(),
        std::string("device-b"),
        std::nullopt,
        std::string("req-publish-takeover")
    };
    auto takeover_future = boost::asio::co_spawn(
        io_context,
        handler.handle_message("device-b", takeover_publish),
        boost::asio::use_future);
    io_context.run();

    auto takeover_response = takeover_future.get();
    ASSERT_TRUE(takeover_response.has_value());
    EXPECT_EQ((*takeover_response)["type"], "error");
    EXPECT_EQ((*takeover_response)["data"]["code"], "UNAUTHORIZED");
}

TEST(MessageHandlerTest, ServicePublishRejectsInvalidAllowedCallers) {
    ScopedEnvVar allow_insecure("SIGNALING_ALLOW_INSECURE_REGISTRATION", "true");
    auto manager = std::make_shared<ConnectionManager>();
    boost::asio::io_context io_context;
    auto device_a_future = boost::asio::co_spawn(
        io_context,
        manager->connect("device-a", std::shared_ptr<WebSocketSession>{}, "", {}),
        boost::asio::use_future);
    io_context.run();
    device_a_future.get();
    io_context.restart();

    MessageHandler handler(manager);
    Message publish{
        MessageType::SERVICE_PUBLISH,
        json{
            {"service_name", "ssh-office"},
            {"allowed_callers", json::array({"device-b", 7})}
        },
        std::chrono::system_clock::now(),
        std::string("device-a"),
        std::nullopt,
        std::string("req-publish-invalid-allowlist")
    };
    auto publish_future = boost::asio::co_spawn(
        io_context,
        handler.handle_message("device-a", publish),
        boost::asio::use_future);
    io_context.run();

    auto publish_response = publish_future.get();
    ASSERT_TRUE(publish_response.has_value());
    EXPECT_EQ((*publish_response)["type"], "error");
    EXPECT_EQ((*publish_response)["data"]["code"], "INVALID_REQUEST");
}

TEST(MessageHandlerTest, ServiceConnectRollsBackSessionWhenDeliveryFails) {
    ScopedEnvVar allow_insecure("SIGNALING_ALLOW_INSECURE_REGISTRATION", "true");
    auto manager = std::make_shared<ConnectionManager>();
    boost::asio::io_context io_context;
    auto owner_future = boost::asio::co_spawn(
        io_context,
        manager->connect("device-a", std::shared_ptr<WebSocketSession>{}, "", {}),
        boost::asio::use_future);
    io_context.run();
    owner_future.get();
    io_context.restart();
    auto caller_future = boost::asio::co_spawn(
        io_context,
        manager->connect("device-b", std::shared_ptr<WebSocketSession>{}, "", {}),
        boost::asio::use_future);
    io_context.run();
    caller_future.get();
    io_context.restart();

    MessageHandler handler(manager);
    Message publish{
        MessageType::SERVICE_PUBLISH,
        json{
            {"service_name", "broken-service"},
            {"allowed_callers", json::array({"device-b"})}
        },
        std::chrono::system_clock::now(),
        std::string("device-a"),
        std::nullopt,
        std::string("req-publish-broken")
    };
    auto publish_future = boost::asio::co_spawn(
        io_context,
        handler.handle_message("device-a", publish),
        boost::asio::use_future);
    io_context.run();
    ASSERT_TRUE(publish_future.get().has_value());
    io_context.restart();

    Message connect{
        MessageType::SERVICE_CONNECT,
        json{{"service_name", "broken-service"}},
        std::chrono::system_clock::now(),
        std::string("device-b"),
        std::nullopt,
        std::string("req-connect-broken")
    };
    auto connect_future = boost::asio::co_spawn(
        io_context,
        handler.handle_message("device-b", connect),
        boost::asio::use_future);
    io_context.run();

    auto connect_response = connect_future.get();
    ASSERT_TRUE(connect_response.has_value());
    EXPECT_EQ((*connect_response)["type"], "error");
    EXPECT_EQ((*connect_response)["data"]["code"], "CONNECTION_FAILED");
    EXPECT_FALSE(manager->get_session_by_devices("device-a", "device-b").has_value());
}

TEST(MessageHandlerTest, ConnectRollsBackSessionWhenDeliveryFails) {
    ScopedEnvVar allow_insecure("SIGNALING_ALLOW_INSECURE_REGISTRATION", "true");
    auto manager = std::make_shared<ConnectionManager>();
    boost::asio::io_context io_context;
    auto target_future = boost::asio::co_spawn(
        io_context,
        manager->connect("device-a", std::shared_ptr<WebSocketSession>{}, "", {}),
        boost::asio::use_future);
    io_context.run();
    target_future.get();
    io_context.restart();
    auto caller_future = boost::asio::co_spawn(
        io_context,
        manager->connect("device-b", std::shared_ptr<WebSocketSession>{}, "", {}),
        boost::asio::use_future);
    io_context.run();
    caller_future.get();
    io_context.restart();

    MessageHandler handler(manager);
    Message connect{
        MessageType::CONNECT,
        json{{"target_device_id", "device-a"}},
        std::chrono::system_clock::now(),
        std::string("device-b"),
        std::nullopt,
        std::string("req-connect-direct")
    };
    auto connect_future = boost::asio::co_spawn(
        io_context,
        handler.handle_message("device-b", connect),
        boost::asio::use_future);
    io_context.run();

    auto connect_response = connect_future.get();
    ASSERT_TRUE(connect_response.has_value());
    EXPECT_EQ((*connect_response)["type"], "error");
    EXPECT_EQ((*connect_response)["data"]["code"], "CONNECTION_FAILED");
    EXPECT_FALSE(manager->get_session_by_devices("device-a", "device-b").has_value());
}

TEST(MessageHandlerTest, DisconnectSessionDoesNotRemoveReplacementConnection) {
    auto manager = std::make_shared<ConnectionManager>();
    boost::asio::io_context io_context;
    auto old_session = std::make_shared<WebSocketSession>(
        boost::asio::ip::tcp::socket(io_context), manager);
    auto new_session = std::make_shared<WebSocketSession>(
        boost::asio::ip::tcp::socket(io_context), manager);

    auto first_connect = boost::asio::co_spawn(
        io_context,
        manager->connect("device-a", old_session, "", {}),
        boost::asio::use_future);
    io_context.run();
    first_connect.get();
    io_context.restart();

    auto second_connect = boost::asio::co_spawn(
        io_context,
        manager->connect("device-a", new_session, "", {}),
        boost::asio::use_future);
    io_context.run();
    second_connect.get();
    io_context.restart();

    auto stale_disconnect = boost::asio::co_spawn(
        io_context,
        manager->disconnect_session("device-a", old_session),
        boost::asio::use_future);
    io_context.run();
    stale_disconnect.get();

    EXPECT_TRUE(manager->is_connected("device-a"));
}

TEST(MessageHandlerTest, RelayRequestRejectsNonMemberCaller) {
    auto manager = std::make_shared<ConnectionManager>();
    ConnectionSession session = manager->create_session("peer-a", "peer-b");
    boost::asio::io_context io_context;
    auto connect_future = boost::asio::co_spawn(
        io_context,
        manager->connect("outsider", std::shared_ptr<WebSocketSession>{}, "", {}),
        boost::asio::use_future);
    io_context.run();
    connect_future.get();
    io_context.restart();

    Message message{
        MessageType::RELAY_REQUEST,
        json{{"session_id", session.session_id}},
        std::chrono::system_clock::now(),
        std::string("outsider"),
        std::nullopt,
        std::string("req-outsider")
    };

    MessageHandler handler(manager);
    auto future = boost::asio::co_spawn(
        io_context,
        handler.handle_message("outsider", message),
        boost::asio::use_future);
    io_context.run();

    auto response = future.get();
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ((*response)["type"], "error");
    EXPECT_EQ((*response)["data"]["code"], "UNAUTHORIZED");
}

TEST(MessageHandlerTest, SecureModeBlocksTempDeviceBeforeRegister) {
    ScopedEnvVar jwt_secret("JWT_SECRET", "unit-test-secret");
    ScopedEnvVar allow_insecure("SIGNALING_ALLOW_INSECURE_REGISTRATION", "false");

    auto manager = std::make_shared<ConnectionManager>();
    boost::asio::io_context io_context;
    auto connect_future = boost::asio::co_spawn(
        io_context,
        manager->connect("temp-unregistered", std::shared_ptr<WebSocketSession>{}, "", {}),
        boost::asio::use_future);
    io_context.run();
    connect_future.get();
    io_context.restart();

    Message message{
        MessageType::SERVICE_PUBLISH,
        json{{"service_name", "ssh-office"}},
        std::chrono::system_clock::now(),
        std::string("temp-unregistered"),
        std::nullopt,
        std::string("req-temp")
    };

    MessageHandler handler(manager);
    auto future = boost::asio::co_spawn(
        io_context,
        handler.handle_message("temp-unregistered", message),
        boost::asio::use_future);
    io_context.run();

    auto response = future.get();
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ((*response)["type"], "error");
    EXPECT_EQ((*response)["data"]["code"], "UNAUTHORIZED");
}

TEST(MessageHandlerTest, HandleHeartbeat) {
    ScopedEnvVar allow_insecure("SIGNALING_ALLOW_INSECURE_REGISTRATION", "true");
    auto manager = std::make_shared<ConnectionManager>();
    boost::asio::io_context io_context;

    auto connect_future = boost::asio::co_spawn(
        io_context,
        manager->connect("device-1", std::shared_ptr<WebSocketSession>{}, "", {}),
        boost::asio::use_future);
    io_context.run();
    connect_future.get();
    io_context.restart();

    Message message{
        MessageType::HEARTBEAT,
        json::object(),
        std::chrono::system_clock::now(),
        std::string("device-1"),
        std::nullopt,
        std::string("req-heartbeat")
    };

    MessageHandler handler(manager);
    auto future = boost::asio::co_spawn(
        io_context,
        handler.handle_message("device-1", message),
        boost::asio::use_future);
    io_context.run();

    auto response = future.get();
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ((*response)["type"], "heartbeat_ack");
}

TEST(MessageHandlerTest, HandlePing) {
    ScopedEnvVar allow_insecure("SIGNALING_ALLOW_INSECURE_REGISTRATION", "true");
    auto manager = std::make_shared<ConnectionManager>();
    boost::asio::io_context io_context;

    auto connect_future = boost::asio::co_spawn(
        io_context,
        manager->connect("device-1", std::shared_ptr<WebSocketSession>{}, "", {}),
        boost::asio::use_future);
    io_context.run();
    connect_future.get();
    io_context.restart();

    Message message{
        MessageType::PING,
        json::object(),
        std::chrono::system_clock::now(),
        std::string("device-1"),
        std::nullopt,
        std::string("req-ping")
    };

    MessageHandler handler(manager);
    auto future = boost::asio::co_spawn(
        io_context,
        handler.handle_message("device-1", message),
        boost::asio::use_future);
    io_context.run();

    auto response = future.get();
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ((*response)["type"], "pong");
}

TEST(MessageHandlerTest, HandleUnregister) {
    ScopedEnvVar allow_insecure("SIGNALING_ALLOW_INSECURE_REGISTRATION", "true");
    auto manager = std::make_shared<ConnectionManager>();
    boost::asio::io_context io_context;

    auto connect_future = boost::asio::co_spawn(
        io_context,
        manager->connect("device-1", std::shared_ptr<WebSocketSession>{}, "", {}),
        boost::asio::use_future);
    io_context.run();
    connect_future.get();
    io_context.restart();

    Message message{
        MessageType::UNREGISTER,
        json::object(),
        std::chrono::system_clock::now(),
        std::string("device-1"),
        std::nullopt,
        std::string("req-unregister")
    };

    MessageHandler handler(manager);
    auto future = boost::asio::co_spawn(
        io_context,
        handler.handle_message("device-1", message),
        boost::asio::use_future);
    io_context.run();

    auto response = future.get();
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ((*response)["type"], "unregister");
}

TEST(MessageHandlerTest, HandleOffer) {
    ScopedEnvVar allow_insecure("SIGNALING_ALLOW_INSECURE_REGISTRATION", "true");
    auto manager = std::make_shared<ConnectionManager>();
    boost::asio::io_context io_context;

    auto f1 = boost::asio::co_spawn(
        io_context,
        manager->connect("device-a", std::shared_ptr<WebSocketSession>{}, "", {}),
        boost::asio::use_future);
    io_context.run();
    f1.get();
    io_context.restart();

    auto f2 = boost::asio::co_spawn(
        io_context,
        manager->connect("device-b", std::shared_ptr<WebSocketSession>{}, "", {}),
        boost::asio::use_future);
    io_context.run();
    f2.get();
    io_context.restart();

    auto session = manager->create_session("device-a", "device-b");

    Message message{
        MessageType::OFFER,
        json{
            {"session_id", session.session_id},
            {"offer", "offer-sdp-content"}
        },
        std::chrono::system_clock::now(),
        std::string("device-a"),
        std::nullopt,
        std::string("req-offer")
    };

    MessageHandler handler(manager);
    auto future = boost::asio::co_spawn(
        io_context,
        handler.handle_message("device-a", message),
        boost::asio::use_future);
    io_context.run();

    auto response = future.get();
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ((*response)["type"], "offer");

    auto updated_session = manager->get_session(session.session_id);
    ASSERT_TRUE(updated_session.has_value());
    EXPECT_EQ(updated_session->offer, "offer-sdp-content");
}

TEST(MessageHandlerTest, HandleAnswer) {
    ScopedEnvVar allow_insecure("SIGNALING_ALLOW_INSECURE_REGISTRATION", "true");
    auto manager = std::make_shared<ConnectionManager>();
    boost::asio::io_context io_context;

    auto f1 = boost::asio::co_spawn(
        io_context,
        manager->connect("device-a", std::shared_ptr<WebSocketSession>{}, "", {}),
        boost::asio::use_future);
    io_context.run();
    f1.get();
    io_context.restart();

    auto f2 = boost::asio::co_spawn(
        io_context,
        manager->connect("device-b", std::shared_ptr<WebSocketSession>{}, "", {}),
        boost::asio::use_future);
    io_context.run();
    f2.get();
    io_context.restart();

    auto session = manager->create_session("device-a", "device-b");

    Message message{
        MessageType::ANSWER,
        json{
            {"session_id", session.session_id},
            {"answer", "answer-sdp-content"}
        },
        std::chrono::system_clock::now(),
        std::string("device-b"),
        std::nullopt,
        std::string("req-answer")
    };

    MessageHandler handler(manager);
    auto future = boost::asio::co_spawn(
        io_context,
        handler.handle_message("device-b", message),
        boost::asio::use_future);
    io_context.run();

    auto response = future.get();
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ((*response)["type"], "answer");

    auto updated_session = manager->get_session(session.session_id);
    ASSERT_TRUE(updated_session.has_value());
    EXPECT_EQ(updated_session->answer, "answer-sdp-content");
}

TEST(MessageHandlerTest, HandleIceCandidate) {
    ScopedEnvVar allow_insecure("SIGNALING_ALLOW_INSECURE_REGISTRATION", "true");
    auto manager = std::make_shared<ConnectionManager>();
    boost::asio::io_context io_context;

    auto f1 = boost::asio::co_spawn(
        io_context,
        manager->connect("device-a", std::shared_ptr<WebSocketSession>{}, "", {}),
        boost::asio::use_future);
    io_context.run();
    f1.get();
    io_context.restart();

    auto f2 = boost::asio::co_spawn(
        io_context,
        manager->connect("device-b", std::shared_ptr<WebSocketSession>{}, "", {}),
        boost::asio::use_future);
    io_context.run();
    f2.get();
    io_context.restart();

    auto session = manager->create_session("device-a", "device-b");

    Message message{
        MessageType::ICE_CANDIDATE,
        json{
            {"session_id", session.session_id},
            {"candidate", "ice-candidate-data"}
        },
        std::chrono::system_clock::now(),
        std::string("device-a"),
        std::nullopt,
        std::string("req-ice")
    };

    MessageHandler handler(manager);
    auto future = boost::asio::co_spawn(
        io_context,
        handler.handle_message("device-a", message),
        boost::asio::use_future);
    io_context.run();

    auto response = future.get();
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ((*response)["type"], "ice_candidate");
}

TEST(MessageHandlerTest, HandleQueryDevice_DeviceOnline) {
    ScopedEnvVar allow_insecure("SIGNALING_ALLOW_INSECURE_REGISTRATION", "true");
    auto manager = std::make_shared<ConnectionManager>();
    boost::asio::io_context io_context;

    auto f1 = boost::asio::co_spawn(
        io_context,
        manager->connect("querier", std::shared_ptr<WebSocketSession>{}, "", {}),
        boost::asio::use_future);
    io_context.run();
    f1.get();
    io_context.restart();

    auto f2 = boost::asio::co_spawn(
        io_context,
        manager->connect("target-device", std::shared_ptr<WebSocketSession>{}, "pk-1", {"tunnel"}),
        boost::asio::use_future);
    io_context.run();
    f2.get();
    io_context.restart();

    Message message{
        MessageType::QUERY_DEVICE,
        json{{"target_device_id", "target-device"}},
        std::chrono::system_clock::now(),
        std::string("querier"),
        std::nullopt,
        std::string("req-query")
    };

    MessageHandler handler(manager);
    auto future = boost::asio::co_spawn(
        io_context,
        handler.handle_message("querier", message),
        boost::asio::use_future);
    io_context.run();

    auto response = future.get();
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ((*response)["type"], "device_info");
    EXPECT_EQ((*response)["data"]["online"], true);
    EXPECT_EQ((*response)["data"]["device_id"], "target-device");
}

TEST(MessageHandlerTest, HandleQueryDevice_DeviceOffline) {
    ScopedEnvVar allow_insecure("SIGNALING_ALLOW_INSECURE_REGISTRATION", "true");
    auto manager = std::make_shared<ConnectionManager>();
    boost::asio::io_context io_context;

    auto f1 = boost::asio::co_spawn(
        io_context,
        manager->connect("querier", std::shared_ptr<WebSocketSession>{}, "", {}),
        boost::asio::use_future);
    io_context.run();
    f1.get();
    io_context.restart();

    Message message{
        MessageType::QUERY_DEVICE,
        json{{"target_device_id", "unknown-device"}},
        std::chrono::system_clock::now(),
        std::string("querier"),
        std::nullopt,
        std::string("req-query")
    };

    MessageHandler handler(manager);
    auto future = boost::asio::co_spawn(
        io_context,
        handler.handle_message("querier", message),
        boost::asio::use_future);
    io_context.run();

    auto response = future.get();
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ((*response)["type"], "device_info");
    EXPECT_EQ((*response)["data"]["online"], false);
}

TEST(MessageHandlerTest, HandleConnect_TargetNotFound) {
    ScopedEnvVar allow_insecure("SIGNALING_ALLOW_INSECURE_REGISTRATION", "true");
    auto manager = std::make_shared<ConnectionManager>();
    boost::asio::io_context io_context;

    auto f1 = boost::asio::co_spawn(
        io_context,
        manager->connect("device-a", std::shared_ptr<WebSocketSession>{}, "", {}),
        boost::asio::use_future);
    io_context.run();
    f1.get();
    io_context.restart();

    Message message{
        MessageType::CONNECT,
        json{{"target_device_id", "nonexistent-device"}},
        std::chrono::system_clock::now(),
        std::string("device-a"),
        std::nullopt,
        std::string("req-connect")
    };

    MessageHandler handler(manager);
    auto future = boost::asio::co_spawn(
        io_context,
        handler.handle_message("device-a", message),
        boost::asio::use_future);
    io_context.run();

    auto response = future.get();
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ((*response)["type"], "error");
}

TEST(MessageHandlerTest, HandleRelayRequest_MissingSessionId) {
    ScopedEnvVar allow_insecure("SIGNALING_ALLOW_INSECURE_REGISTRATION", "true");
    auto manager = std::make_shared<ConnectionManager>();
    boost::asio::io_context io_context;

    auto f1 = boost::asio::co_spawn(
        io_context,
        manager->connect("device-a", std::shared_ptr<WebSocketSession>{}, "", {}),
        boost::asio::use_future);
    io_context.run();
    f1.get();
    io_context.restart();

    Message message{
        MessageType::RELAY_REQUEST,
        json::object(),
        std::chrono::system_clock::now(),
        std::string("device-a"),
        std::nullopt,
        std::string("req-relay")
    };

    MessageHandler handler(manager);
    auto future = boost::asio::co_spawn(
        io_context,
        handler.handle_message("device-a", message),
        boost::asio::use_future);
    io_context.run();

    auto response = future.get();
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ((*response)["type"], "error");
    EXPECT_EQ((*response)["data"]["code"], "INVALID_REQUEST");
}

TEST(MessageHandlerTest, RegisterInsecureMode) {
    ScopedEnvVar allow_insecure("SIGNALING_ALLOW_INSECURE_REGISTRATION", "true");
    auto manager = std::make_shared<ConnectionManager>();
    boost::asio::io_context io_context;

    auto f1 = boost::asio::co_spawn(
        io_context,
        manager->connect("temp-client-x", std::shared_ptr<WebSocketSession>{}, "", {}),
        boost::asio::use_future);
    io_context.run();
    f1.get();
    io_context.restart();

    Message message{
        MessageType::REGISTER,
        json{
            {"device_id", "my-device-123"},
            {"public_key", "pk-123"},
            {"capabilities", json::array({"tunnel"})}
        },
        std::chrono::system_clock::now(),
        std::string("temp-client-x"),
        std::nullopt,
        std::string("req-register")
    };

    MessageHandler handler(manager);
    auto future = boost::asio::co_spawn(
        io_context,
        handler.handle_message("temp-client-x", message),
        boost::asio::use_future);
    io_context.run();

    auto response = future.get();
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ((*response)["type"], "registered");
    EXPECT_EQ((*response)["data"]["device_id"], "my-device-123");
}

}  // namespace
}  // namespace signaling

