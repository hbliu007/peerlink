#pragma once

#include "models.hpp"
#include "p2p/servers/relay/rate_limiter.hpp"
#include <boost/asio/awaitable.hpp>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace signaling {

namespace asio = boost::asio;

class ConnectionManager : public std::enable_shared_from_this<ConnectionManager> {
public:
    ConnectionManager();
    ~ConnectionManager() = default;

    // Rate limiting
    bool CheckRateLimit(const std::string& client_ip);
    void SetRateLimiter(std::unique_ptr<p2p::relay::RateLimiter> rate_limiter);


    // Connection management
    asio::awaitable<void> connect(
        std::string device_id,
        std::shared_ptr<WebSocketSession> session,
        std::string public_key,
        std::vector<std::string> capabilities,
        json metadata = json::object()
    );

    asio::awaitable<void> disconnect(const std::string& device_id);
    asio::awaitable<void> disconnect_session(const std::string& device_id,
                                             const std::shared_ptr<WebSocketSession>& session);

    std::optional<DeviceInfo> get_device(const std::string& device_id) const;
    bool is_connected(const std::string& device_id) const;
    std::unordered_set<std::string> get_all_devices() const;
    std::size_t device_count() const;
    std::size_t session_count() const;
    std::size_t relay_session_count() const;

    asio::awaitable<bool> register_device(
        const std::string& current_device_id,
        std::string requested_device_id,
        std::string public_key,
        std::vector<std::string> capabilities,
        json metadata = json::object()
    );

    // Message sending
    asio::awaitable<bool> send_message(
        const std::string& device_id,
        const json& message
    );

    asio::awaitable<int> broadcast(
        const json& message,
        const std::unordered_set<std::string>& exclude = {}
    );

    asio::awaitable<bool> send_error(
        const std::string& device_id,
        ErrorCode code,
        const std::string& message,
        const std::optional<std::string>& request_id = std::nullopt
    );

    // Session management
    ConnectionSession create_session(
        std::string device_a,
        std::string device_b
    );

    std::optional<ConnectionSession> get_session(
        const std::string& session_id
    ) const;

    std::optional<ConnectionSession> get_session_by_devices(
        const std::string& device_a,
        const std::string& device_b
    ) const;

    bool remove_session(const std::string& session_id);

    void update_session_status(
        const std::string& session_id,
        ConnectionStatus status
    );

    void set_session_offer(
        const std::string& session_id,
        std::string offer
    );

    void set_session_answer(
        const std::string& session_id,
        std::string answer
    );

    void add_ice_candidate(
        const std::string& session_id,
        const std::string& device_id,
        json candidate
    );

    void set_relay_mode(const std::string& session_id);

    // Published services
    void publish_service(PublishedService service);
    bool unpublish_service(const std::string& owner_device_id, const std::string& service_name);
    std::optional<PublishedService> get_service(const std::string& service_name) const;
    std::size_t service_count() const;

    // Pending requests
    void add_pending_request(
        const std::string& session_id,
        std::string requester
    );

    std::optional<std::string> get_pending_requester(
        const std::string& session_id
    ) const;

    void remove_pending_request(const std::string& session_id);

    // Heartbeat
    asio::awaitable<bool> update_heartbeat(const std::string& device_id);
    asio::awaitable<int> cleanup_stale(int timeout_seconds);

private:
    // Active device connections
    std::unordered_map<std::string, DeviceInfo> devices_;
    mutable std::shared_mutex devices_mutex_;

    // Active connection sessions
    std::unordered_map<std::string, ConnectionSession> sessions_;
    mutable std::shared_mutex sessions_mutex_;

    std::unordered_map<std::string, PublishedService> services_;
    mutable std::shared_mutex services_mutex_;

    // Pending connection requests
    std::unordered_map<std::string, std::string> pending_requests_;
    mutable std::shared_mutex pending_mutex_;

    // Rate limiter
    std::unique_ptr<p2p::relay::RateLimiter> rate_limiter_;
};

} // namespace signaling