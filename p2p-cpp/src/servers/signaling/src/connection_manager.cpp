#include "connection_manager.hpp"
#include "websocket_session.hpp"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace asio = boost::asio;

namespace signaling {

ConnectionManager::ConnectionManager() {
    // Initialize rate limiter with default config
    // 20 messages per second, burst 50, ban threshold 5, ban duration 300s
    p2p::relay::RateLimitConfig rate_config(20, 50, 5, 300);
    rate_limiter_ = std::make_unique<p2p::relay::RateLimiter>(rate_config);
}

bool ConnectionManager::CheckRateLimit(const std::string& client_ip) {
    if (!rate_limiter_) {
        return true;
    }
    return rate_limiter_->AllowRequest(client_ip);
}

void ConnectionManager::SetRateLimiter(std::unique_ptr<p2p::relay::RateLimiter> rate_limiter) {
    rate_limiter_ = std::move(rate_limiter);
}


// Connection management
asio::awaitable<void> ConnectionManager::connect(
    std::string device_id,
    std::shared_ptr<WebSocketSession> session,
    std::string public_key,
    std::vector<std::string> capabilities,
    json metadata
) {
    {
        std::unique_lock lock(devices_mutex_);

        // Check if device already connected
        auto it = devices_.find(device_id);
        if (it != devices_.end()) {
            // Close old connection
            auto old_session = it->second.session;
            if (old_session) {
                co_await old_session->close();
            }
        }

        // Create new device info
        DeviceInfo device;
        device.device_id = device_id;
        device.session = std::move(session);
        device.public_key = std::move(public_key);
        device.capabilities = std::move(capabilities);
        device.metadata = std::move(metadata);
        device.connected_at = std::chrono::system_clock::now();
        device.last_heartbeat = std::chrono::system_clock::now();

        devices_[device_id] = std::move(device);
    }

    co_return;
}

asio::awaitable<void> ConnectionManager::disconnect(const std::string& device_id) {
    co_await disconnect_session(device_id, nullptr);
}

asio::awaitable<void> ConnectionManager::disconnect_session(
    const std::string& device_id,
    const std::shared_ptr<WebSocketSession>& session) {
    {
        std::unique_lock lock(devices_mutex_);

        auto it = devices_.find(device_id);
        if (it == devices_.end()) {
            co_return;
        }

        if (session && it->second.session != session) {
            co_return;
        }

        devices_.erase(it);
    }

    // Clean up pending requests
    {
        std::unique_lock lock(pending_mutex_);
        std::vector<std::string> to_remove;

        for (const auto& [session_id, requester] : pending_requests_) {
            if (requester == device_id) {
                to_remove.push_back(session_id);
            }
        }

        for (const auto& session_id : to_remove) {
            pending_requests_.erase(session_id);
        }
    }

    // Update related sessions
    {
        std::unique_lock lock(sessions_mutex_);
        for (auto& [_, session] : sessions_) {
            if (session.device_a == device_id || session.device_b == device_id) {
                session.status = ConnectionStatus::DISCONNECTED;
            }
        }
    }

    {
        std::unique_lock lock(services_mutex_);
        for (auto it = services_.begin(); it != services_.end();) {
            if (it->second.owner_device_id == device_id) {
                it = services_.erase(it);
            } else {
                ++it;
            }
        }
    }

    co_return;
}

std::optional<DeviceInfo> ConnectionManager::get_device(
    const std::string& device_id
) const {
    std::shared_lock lock(devices_mutex_);
    auto it = devices_.find(device_id);
    return it != devices_.end()
        ? std::optional{it->second}
        : std::nullopt;
}

bool ConnectionManager::is_connected(const std::string& device_id) const {
    std::shared_lock lock(devices_mutex_);
    return devices_.find(device_id) != devices_.end();
}

std::size_t ConnectionManager::device_count() const {
    std::shared_lock lock(devices_mutex_);
    return devices_.size();
}

std::size_t ConnectionManager::session_count() const {
    std::shared_lock lock(sessions_mutex_);
    return sessions_.size();
}

std::size_t ConnectionManager::relay_session_count() const {
    std::shared_lock lock(sessions_mutex_);
    std::size_t count = 0;
    for (const auto& [_, session] : sessions_) {
        if (session.use_relay) {
            ++count;
        }
    }
    return count;
}

std::unordered_set<std::string> ConnectionManager::get_all_devices() const {
    std::shared_lock lock(devices_mutex_);
    std::unordered_set<std::string> result;
    for (const auto& [device_id, _] : devices_) {
        result.insert(device_id);
    }
    return result;
}

asio::awaitable<bool> ConnectionManager::register_device(
    const std::string& current_device_id,
    std::string requested_device_id,
    std::string public_key,
    std::vector<std::string> capabilities,
    json metadata) {
    std::shared_ptr<WebSocketSession> session_to_close;
    {
        std::unique_lock lock(devices_mutex_);
        auto current_it = devices_.find(current_device_id);
        if (current_it == devices_.end()) {
            co_return false;
        }

        DeviceInfo device = current_it->second;
        device.device_id = requested_device_id;
        device.public_key = std::move(public_key);
        device.capabilities = std::move(capabilities);
        device.metadata = std::move(metadata);
        device.last_heartbeat = std::chrono::system_clock::now();
        device.status = ConnectionStatus::CONNECTED;

        auto target_it = devices_.find(requested_device_id);
        if (target_it != devices_.end() && target_it->second.session != device.session) {
            session_to_close = target_it->second.session;
        }

        if (requested_device_id != current_device_id) {
            devices_.erase(current_it);
        }
        devices_[requested_device_id] = std::move(device);
    }

    if (session_to_close) {
        co_await session_to_close->close();
    }

    {
        std::unique_lock lock(sessions_mutex_);
        for (auto& [_, session] : sessions_) {
            if (session.device_a == current_device_id) {
                session.device_a = requested_device_id;
            }
            if (session.device_b == current_device_id) {
                session.device_b = requested_device_id;
            }
        }
    }

    {
        std::unique_lock lock(services_mutex_);
        std::vector<std::pair<std::string, PublishedService>> replacements;
        for (auto it = services_.begin(); it != services_.end();) {
            if (it->second.owner_device_id == current_device_id) {
                auto service = it->second;
                service.owner_device_id = requested_device_id;
                replacements.emplace_back(it->first, std::move(service));
                it = services_.erase(it);
            } else {
                ++it;
            }
        }
        for (auto& [name, service] : replacements) {
            services_[name] = std::move(service);
        }
    }

    co_return true;
}

// Message sending
asio::awaitable<bool> ConnectionManager::send_message(
    const std::string& device_id,
    const json& message
) {
    std::shared_ptr<WebSocketSession> session;

    {
        std::shared_lock lock(devices_mutex_);
        auto it = devices_.find(device_id);
        if (it == devices_.end()) {
            co_return false;
        }
        session = it->second.session;
    }

    if (!session) {
        co_return false;
    }

    try {
        co_await session->send(message);
        co_return true;
    } catch (const std::exception&) {
        // Cannot use co_await in catch block, schedule disconnect
        // Note: Using lambda to avoid detached token issues
        boost::asio::co_spawn(
            session->get_executor(),
            disconnect_session(device_id, session),
            [](std::exception_ptr) {}
        );
        co_return false;
    }
}

asio::awaitable<int> ConnectionManager::broadcast(
    const json& message,
    const std::unordered_set<std::string>& exclude
) {
    int count = 0;

    std::vector<std::string> device_ids;
    {
        std::shared_lock lock(devices_mutex_);
        for (const auto& [device_id, _] : devices_) {
            if (exclude.find(device_id) == exclude.end()) {
                device_ids.push_back(device_id);
            }
        }
    }

    for (const auto& device_id : device_ids) {
        if (co_await send_message(device_id, message)) {
            ++count;
        }
    }

    co_return count;
}

asio::awaitable<bool> ConnectionManager::send_error(
    const std::string& device_id,
    ErrorCode code,
    const std::string& message,
    const std::optional<std::string>& request_id
) {
    ErrorResponse error{code, message, request_id};
    co_return co_await send_message(device_id, error.to_json());
}

// Session management
ConnectionSession ConnectionManager::create_session(
    std::string device_a,
    std::string device_b
) {
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    std::string session_id = boost::uuids::to_string(uuid);

    ConnectionSession session;
    session.session_id = session_id;
    session.device_a = std::move(device_a);
    session.device_b = std::move(device_b);
    session.created_at = std::chrono::system_clock::now();

    {
        std::unique_lock lock(sessions_mutex_);
        sessions_[session_id] = session;
    }

    return session;
}

std::optional<ConnectionSession> ConnectionManager::get_session(
    const std::string& session_id
) const {
    std::shared_lock lock(sessions_mutex_);
    auto it = sessions_.find(session_id);
    return it != sessions_.end()
        ? std::optional{it->second}
        : std::nullopt;
}

std::optional<ConnectionSession> ConnectionManager::get_session_by_devices(
    const std::string& device_a,
    const std::string& device_b
) const {
    std::shared_lock lock(sessions_mutex_);
    for (const auto& [_, session] : sessions_) {
        if ((session.device_a == device_a && session.device_b == device_b) ||
            (session.device_a == device_b && session.device_b == device_a)) {
            return session;
        }
    }
    return std::nullopt;
}

bool ConnectionManager::remove_session(const std::string& session_id) {
    std::unique_lock lock(sessions_mutex_);
    return sessions_.erase(session_id) > 0;
}

void ConnectionManager::update_session_status(
    const std::string& session_id,
    ConnectionStatus status
) {
    std::unique_lock lock(sessions_mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        it->second.status = status;
    }
}

void ConnectionManager::set_session_offer(
    const std::string& session_id,
    std::string offer
) {
    std::unique_lock lock(sessions_mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        it->second.offer = std::move(offer);
    }
}

void ConnectionManager::set_session_answer(
    const std::string& session_id,
    std::string answer
) {
    std::unique_lock lock(sessions_mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        it->second.answer = std::move(answer);
    }
}

void ConnectionManager::add_ice_candidate(
    const std::string& session_id,
    const std::string& device_id,
    json candidate
) {
    std::unique_lock lock(sessions_mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        if (it->second.device_a == device_id) {
            it->second.ice_candidates_a.push_back(std::move(candidate));
        } else if (it->second.device_b == device_id) {
            it->second.ice_candidates_b.push_back(std::move(candidate));
        }
    }
}

void ConnectionManager::set_relay_mode(const std::string& session_id) {
    std::unique_lock lock(sessions_mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        it->second.use_relay = true;
    }
}

void ConnectionManager::publish_service(PublishedService service) {
    std::unique_lock lock(services_mutex_);
    services_[service.name] = std::move(service);
}

bool ConnectionManager::unpublish_service(const std::string& owner_device_id,
                                          const std::string& service_name) {
    std::unique_lock lock(services_mutex_);
    auto it = services_.find(service_name);
    if (it == services_.end() || it->second.owner_device_id != owner_device_id) {
        return false;
    }
    services_.erase(it);
    return true;
}

std::optional<PublishedService> ConnectionManager::get_service(const std::string& service_name) const {
    std::shared_lock lock(services_mutex_);
    auto it = services_.find(service_name);
    return it != services_.end() ? std::optional{it->second} : std::nullopt;
}

std::size_t ConnectionManager::service_count() const {
    std::shared_lock lock(services_mutex_);
    return services_.size();
}

// Pending requests
void ConnectionManager::add_pending_request(
    const std::string& session_id,
    std::string requester
) {
    std::unique_lock lock(pending_mutex_);
    pending_requests_[session_id] = std::move(requester);
}

std::optional<std::string> ConnectionManager::get_pending_requester(
    const std::string& session_id
) const {
    std::shared_lock lock(pending_mutex_);
    auto it = pending_requests_.find(session_id);
    return it != pending_requests_.end()
        ? std::optional{it->second}
        : std::nullopt;
}

void ConnectionManager::remove_pending_request(const std::string& session_id) {
    std::unique_lock lock(pending_mutex_);
    pending_requests_.erase(session_id);
}

// Heartbeat
asio::awaitable<bool> ConnectionManager::update_heartbeat(
    const std::string& device_id
) {
    std::unique_lock lock(devices_mutex_);
    auto it = devices_.find(device_id);
    if (it != devices_.end()) {
        it->second.last_heartbeat = std::chrono::system_clock::now();
        co_return true;
    }
    co_return false;
}

asio::awaitable<int> ConnectionManager::cleanup_stale(int timeout_seconds) {
    auto now = std::chrono::system_clock::now();
    auto timeout = std::chrono::seconds(timeout_seconds);

    std::vector<std::string> to_remove;

    {
        std::shared_lock lock(devices_mutex_);
        for (const auto& [device_id, device] : devices_) {
            if (now - device.last_heartbeat > timeout) {
                to_remove.push_back(device_id);
            }
        }
    }

    for (const auto& device_id : to_remove) {
        co_await disconnect(device_id);
    }

    co_return static_cast<int>(to_remove.size());
}

} // namespace signaling