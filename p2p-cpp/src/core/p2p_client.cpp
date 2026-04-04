#include "p2p/core/p2p_client.hpp"
#include "p2p/transport/relay_transport.hpp"
#include "p2p/utils/logger.hpp"

namespace asio = boost::asio;

namespace p2p {
namespace core {

const char* P2PClient::ToString(RelayMode mode) {
    switch (mode) {
        case RelayMode::AUTO:
            return "auto";
        case RelayMode::RELAY_PREFERRED:
            return "relay-preferred";
        case RelayMode::RELAY_ONLY:
            return "relay-only";
        case RelayMode::DIRECT_ONLY:
            return "direct-only";
    }
    return "unknown";
}

const char* P2PClient::ToString(ConnectionPath path) {
    switch (path) {
        case ConnectionPath::NONE:
            return "none";
        case ConnectionPath::DIRECT_P2P:
            return "direct-p2p";
        case ConnectionPath::RELAY:
            return "relay";
    }
    return "unknown";
}

const char* P2PClient::ToString(ConnectionFailureReason reason) {
    switch (reason) {
        case ConnectionFailureReason::NONE:
            return "none";
        case ConnectionFailureReason::NOT_RUNNING:
            return "not-running";
        case ConnectionFailureReason::ALREADY_CONNECTED:
            return "already-connected";
        case ConnectionFailureReason::NAT_DETECTION_FAILED:
            return "nat-detection-failed";
        case ConnectionFailureReason::DIRECT_PATH_UNAVAILABLE:
            return "direct-path-unavailable";
        case ConnectionFailureReason::DIRECT_PATH_NOT_IMPLEMENTED:
            return "direct-path-not-implemented";
        case ConnectionFailureReason::RELAY_NOT_CONFIGURED:
            return "relay-not-configured";
        case ConnectionFailureReason::RELAY_REGISTRATION_FAILED:
            return "relay-registration-failed";
        case ConnectionFailureReason::RELAY_CONNECT_FAILED:
            return "relay-connect-failed";
        case ConnectionFailureReason::NO_TRANSPORT_AVAILABLE:
            return "no-transport-available";
    }
    return "unknown";
}

P2PClient::P2PClient(boost::asio::io_context& io_context,
                     const std::string& did,
                     const P2PConfig& config)
    : io_context_(io_context)
    , did_(did)
    , config_(config)
    , state_(ConnectionState::DISCONNECTED)
    , active_path_(ConnectionPath::NONE)
    , last_failure_reason_(ConnectionFailureReason::NONE)
    , used_relay_fallback_(false)
    , next_channel_id_(1)
    , keepalive_timer_(io_context)
    , running_(false)
    , closing_(false)
{
}

P2PClient::~P2PClient() {
    close();
}

void P2PClient::initialize(std::function<void(const std::error_code&)> callback) {
    if (running_) {
        set_failure(ConnectionFailureReason::ALREADY_CONNECTED,
                    "Client is already initialized and running");
        callback(std::make_error_code(std::errc::already_connected));
        return;
    }

    active_path_ = ConnectionPath::NONE;
    last_failure_reason_ = ConnectionFailureReason::NONE;
    last_failure_detail_.clear();
    used_relay_fallback_ = false;

    // RELAY_ONLY mode: skip STUN entirely, go straight to relay
    if (config_.relay_mode == RelayMode::RELAY_ONLY) {
        LOG_INFO("[P2PClient] RELAY_ONLY mode — skipping STUN detection");
        nat_type_ = nat::NATType::BLOCKED;
        auto weak_self = weak_from_this();
        init_relay_transport([weak_self, callback](const std::error_code& ec) {
            auto self = weak_self.lock();
            if (!self) return;
            if (ec) {
                self->set_failure(ConnectionFailureReason::RELAY_REGISTRATION_FAILED,
                            "Failed to initialize relay transport: " + ec.message());
                callback(ec);
                return;
            }
            self->running_ = true;
            callback(std::error_code());
        });
        return;
    }

    // AUTO / DIRECT_ONLY mode: try STUN first
    auto weak_self = weak_from_this();
    detect_nat([weak_self, callback](const std::error_code& ec) {
        auto self = weak_self.lock();
        if (!self) return;

        if (ec || self->nat_type_ == nat::NATType::BLOCKED) {
            if (ec) {
                self->set_failure(ConnectionFailureReason::NAT_DETECTION_FAILED,
                            "STUN/NAT detection failed: " + ec.message());
            }
            // STUN failed or BLOCKED — non-fatal in AUTO mode
            if (self->config_.relay_mode == RelayMode::DIRECT_ONLY) {
                LOG_ERROR("[P2PClient] STUN failed in DIRECT_ONLY mode — aborting");
                self->set_failure(ConnectionFailureReason::DIRECT_PATH_UNAVAILABLE,
                            "DIRECT_ONLY mode requires a working direct path");
                callback(ec ? ec : std::make_error_code(std::errc::network_unreachable));
                return;
            }

            LOG_WARN("[P2PClient] STUN failed/BLOCKED — falling back to relay");

            // Try relay as fallback
            self->init_relay_transport([weak_self, callback](const std::error_code& relay_ec) {
                auto self2 = weak_self.lock();
                if (!self2) return;
                if (relay_ec) {
                    LOG_ERROR("[P2PClient] Relay fallback also failed");
                    self2->set_failure(ConnectionFailureReason::RELAY_REGISTRATION_FAILED,
                                "Relay fallback initialization failed: " + relay_ec.message());
                    callback(relay_ec);
                    return;
                }
                self2->used_relay_fallback_ = true;
                self2->running_ = true;
                callback(std::error_code());
            });
            return;
        }

        // STUN succeeded — initialize UDP transport
        self->udp_transport_ = std::make_shared<transport::UDPTransport>(
            self->io_context_, self->config_.local_port
        );

        std::error_code transport_ec;
        self->udp_transport_->start(transport_ec);

        if (transport_ec) {
            callback(transport_ec);
            return;
        }

        self->udp_transport_->set_receive_callback(
            [weak_self](const std::vector<uint8_t>& data, const auto& /*endpoint*/) {
                auto s = weak_self.lock();
                if (s) s->handle_received_message(data);
            }
        );

        // Also init relay for fallback if tcp_relay_server is configured
        if (!self->config_.tcp_relay_server.empty()) {
            self->init_relay_transport([](const std::error_code& ec) {
                if (ec) {
                    LOG_WARN("[P2PClient] Relay pre-registration failed (non-fatal): {}",
                              ec.message());
                }
            });
        }

        self->running_ = true;
        callback(std::error_code());
    });
}

void P2PClient::detect_nat(std::function<void(const std::error_code&)> callback) {
    auto weak_self = weak_from_this();
    nat::detect_nat_type(
        io_context_,
        config_.stun_server,
        config_.stun_port,
        [weak_self, callback](const nat::NATDetectionResult& result) {
            auto self = weak_self.lock();
            if (!self) return;

            self->nat_type_ = result.nat_type;

            if (result.public_ip && result.public_port) {
                self->public_addr_ = std::make_pair(*result.public_ip, *result.public_port);
            }

            LOG_INFO("[P2PClient] NAT type detected: {}", static_cast<int>(result.nat_type));
            if (result.nat_type == nat::NATType::BLOCKED) {
                self->set_failure(ConnectionFailureReason::DIRECT_PATH_UNAVAILABLE,
                            "NAT detection reported a blocked direct path");
            } else {
                self->last_failure_reason_ = ConnectionFailureReason::NONE;
                self->last_failure_detail_.clear();
            }

            // BLOCKED is no longer fatal — caller decides how to handle
            callback(std::error_code());
        }
    );
}

void P2PClient::init_relay_transport(std::function<void(const std::error_code&)> callback) {
    if (config_.tcp_relay_server.empty()) {
        LOG_ERROR("[P2PClient] No TCP relay server configured");
        set_failure(ConnectionFailureReason::RELAY_NOT_CONFIGURED,
                    "tcp_relay_server is empty");
        callback(std::make_error_code(std::errc::no_such_device_or_address));
        return;
    }

    relay_transport_ = std::make_shared<transport::RelayTransport>(
        io_context_, config_.tcp_relay_server, config_.tcp_relay_port, did_
    );

    auto weak_self = weak_from_this();
    relay_transport_->set_event_callback(
        [weak_self](const std::string& event, const std::string& detail) {
            auto self = weak_self.lock();
            if (!self) return;
            LOG_INFO("[P2PClient] Relay event: {} — {}", event, detail);
            if (event == "connected" && detail.substr(0, 8) == "incoming" &&
                self->state_ != ConnectionState::CONNECTED_RELAY) {
                // Incoming connection from peer via relay
                self->active_path_ = ConnectionPath::RELAY;
                self->last_failure_reason_ = ConnectionFailureReason::NONE;
                self->last_failure_detail_.clear();
                self->state_ = ConnectionState::CONNECTED_RELAY;
                self->start_keepalive();
                if (self->on_connected_) {
                    self->on_connected_();
                }
            }
            if (event == "disconnected" && !self->closing_ &&
                self->state_ == ConnectionState::CONNECTED_RELAY) {
                self->set_failure(ConnectionFailureReason::RELAY_CONNECT_FAILED,
                            "Relay disconnected while active: " + detail);
                self->state_ = ConnectionState::DISCONNECTED;
                self->active_path_ = ConnectionPath::NONE;
                if (self->on_disconnected_) {
                    self->on_disconnected_();
                }
            }
        }
    );

    relay_transport_->set_receive_callback(
        [weak_self](const std::vector<uint8_t>& data) {
            auto self = weak_self.lock();
            if (self) self->handle_received_message(data);
        }
    );

    relay_transport_->start(callback);
}

void P2PClient::connect(const std::string& peer_did,
                        std::function<void(const std::error_code&)> callback) {
    if (!running_) {
        set_failure(ConnectionFailureReason::NOT_RUNNING,
                    "initialize() must succeed before connect()");
        callback(std::make_error_code(std::errc::not_connected));
        return;
    }

    if (state_ == ConnectionState::CONNECTED_P2P ||
        state_ == ConnectionState::CONNECTED_RELAY) {
        set_failure(ConnectionFailureReason::ALREADY_CONNECTED,
                    "Client already has an active connection");
        callback(std::make_error_code(std::errc::already_connected));
        return;
    }

    state_ = ConnectionState::CONNECTING;

    PeerInfo peer;
    peer.did = peer_did;
    current_peer_ = peer;

    const bool direct_available = has_direct_path();
    const bool relay_available = has_registered_relay();

    LOG_INFO("[P2PClient] connect({}) mode={} direct_available={} relay_available={}",
              peer_did, ToString(config_.relay_mode),
              direct_available ? "true" : "false",
              relay_available ? "true" : "false");

    if (config_.relay_mode == RelayMode::RELAY_ONLY) {
        if (relay_available) {
            attempt_relay_connect(peer_did, false, callback);
            return;
        }
        set_failure(ConnectionFailureReason::RELAY_NOT_CONFIGURED,
                    "RELAY_ONLY mode requires a registered relay transport");
        state_ = ConnectionState::FAILED;
        callback(std::make_error_code(std::errc::network_unreachable));
        return;
    }

    if (config_.relay_mode == RelayMode::RELAY_PREFERRED) {
        if (relay_available) {
            attempt_relay_connect(peer_did, false, callback);
            return;
        }
        if (direct_available) {
            used_relay_fallback_ = false;
            complete_direct_connect(callback);
            return;
        }
        set_failure(ConnectionFailureReason::NO_TRANSPORT_AVAILABLE,
                    "Neither relay nor direct path is currently available");
        state_ = ConnectionState::FAILED;
        callback(std::make_error_code(std::errc::network_unreachable));
        return;
    }

    if (config_.relay_mode == RelayMode::DIRECT_ONLY) {
        if (!direct_available) {
            set_failure(ConnectionFailureReason::DIRECT_PATH_UNAVAILABLE,
                        "DIRECT_ONLY mode requires a prepared UDP/direct path");
            state_ = ConnectionState::FAILED;
            callback(std::make_error_code(std::errc::network_unreachable));
            return;
        }
        used_relay_fallback_ = false;
        complete_direct_connect(callback);
        return;
    }

    // AUTO: prefer direct, then fall back to relay.
    if (direct_available) {
        used_relay_fallback_ = false;
        complete_direct_connect(callback);
        return;
    }
    if (relay_available) {
        used_relay_fallback_ = true;
        set_failure(ConnectionFailureReason::DIRECT_PATH_UNAVAILABLE,
                    "AUTO mode fell back to relay because no direct path is ready");
        attempt_relay_connect(peer_did, true, callback);
        return;
    }

    LOG_ERROR("[P2PClient] No transport available for connection");
    set_failure(ConnectionFailureReason::NO_TRANSPORT_AVAILABLE,
                "Neither direct nor relay transport is available");
    state_ = ConnectionState::FAILED;
    callback(std::make_error_code(std::errc::network_unreachable));
}

void P2PClient::send_data(int channel_id,
                          const std::vector<uint8_t>& data,
                          std::function<void(const std::error_code&)> callback) {
    if (!is_connected()) {
        if (callback) {
            callback(std::make_error_code(std::errc::not_connected));
        }
        return;
    }

    // Create channel data message
    protocol::ChannelDataMessage msg(did_, current_peer_->did, channel_id, data);
    auto encoded = msg.encode();

    if (state_ == ConnectionState::CONNECTED_RELAY && relay_transport_) {
        // Send via relay
        relay_transport_->send(encoded, [callback](const std::error_code& ec) {
            if (callback) {
                callback(ec);
            }
        });
    } else if (udp_transport_) {
        // Send via UDP
        udp_transport_->send(encoded, [callback](const std::error_code& ec) {
            if (callback) {
                callback(ec);
            }
        });
    } else {
        if (callback) {
            callback(std::make_error_code(std::errc::not_connected));
        }
    }
}

int P2PClient::create_channel() {
    int channel_id = next_channel_id_++;
    channels_[channel_id] = std::queue<std::vector<uint8_t>>();
    return channel_id;
}

void P2PClient::close_channel(int channel_id) {
    channels_.erase(channel_id);
}

void P2PClient::close() {
    if (!running_ || closing_) {
        return;
    }

    closing_ = true;
    running_ = false;
    state_ = ConnectionState::DISCONNECTED;
    active_path_ = ConnectionPath::NONE;

    // Stop keepalive
    keepalive_timer_.cancel();

    // Close transports
    if (udp_transport_) {
        udp_transport_->stop();
    }
    if (relay_transport_) {
        relay_transport_->stop();
    }

    // Notify disconnected (exactly once)
    if (on_disconnected_) {
        on_disconnected_();
    }

    closing_ = false;
}

void P2PClient::reset_for_retry() {
    state_ = ConnectionState::DISCONNECTED;
    active_path_ = ConnectionPath::NONE;
    keepalive_timer_.cancel();
}

void P2PClient::start_keepalive() {
    if (!running_) {
        return;
    }

    keepalive_timer_.expires_after(config_.keepalive_interval);
    auto weak_self = weak_from_this();
    keepalive_timer_.async_wait([weak_self](const boost::system::error_code& ec) {
        auto self = weak_self.lock();
        if (!self) return;
        if (!ec && self->running_) {
            self->send_keepalive();
            self->start_keepalive();  // Schedule next keepalive
        }
    });
}

void P2PClient::send_keepalive() {
    if (!is_connected() || !current_peer_) {
        return;
    }

    protocol::KeepaliveMessage msg(did_, current_peer_->did);
    auto encoded = msg.encode();

    if (state_ == ConnectionState::CONNECTED_RELAY && relay_transport_) {
        relay_transport_->send(encoded, [](const std::error_code&) {
            // Ignore errors for keepalive
        });
    } else if (udp_transport_) {
        udp_transport_->send(encoded, [](const std::error_code&) {
            // Ignore errors for keepalive
        });
    }
}

void P2PClient::set_failure(ConnectionFailureReason reason, const std::string& detail) {
    last_failure_reason_ = reason;
    last_failure_detail_ = detail;
}

bool P2PClient::has_direct_path() const {
    return udp_transport_ != nullptr &&
           nat_type_.has_value() &&
           nat_type_.value() != nat::NATType::BLOCKED;
}

bool P2PClient::has_registered_relay() const {
    return relay_transport_ != nullptr && relay_transport_->is_registered();
}

void P2PClient::attempt_relay_connect(
    const std::string& peer_did,
    bool fallback,
    std::function<void(const std::error_code&)> callback) {
    LOG_INFO("[P2PClient] Connecting to {} via relay{}", peer_did,
              fallback ? " (fallback)" : "");
    auto weak_self = weak_from_this();
    relay_transport_->connect_to_peer(peer_did,
        [weak_self, callback, fallback](const std::error_code& ec) {
            auto self = weak_self.lock();
            if (!self) return;
            if (ec) {
                self->state_ = ConnectionState::FAILED;
                self->set_failure(ConnectionFailureReason::RELAY_CONNECT_FAILED,
                            std::string("Relay connect failed") +
                            (fallback ? " after fallback: " : ": ") + ec.message());
                callback(ec);
                return;
            }

            self->active_path_ = ConnectionPath::RELAY;
            self->last_failure_reason_ = ConnectionFailureReason::NONE;
            self->last_failure_detail_.clear();
            self->used_relay_fallback_ = fallback;
            self->state_ = ConnectionState::CONNECTED_RELAY;
            self->start_keepalive();

            if (self->on_connected_) {
                self->on_connected_();
            }
            callback(std::error_code());
        });
}

void P2PClient::complete_direct_connect(std::function<void(const std::error_code&)> callback) {
    if (!current_peer_ || !current_peer_->public_ip || !current_peer_->public_port) {
        LOG_ERROR("[P2PClient] Cannot punch — peer public address unknown");
        set_failure(ConnectionFailureReason::DIRECT_PATH_UNAVAILABLE,
                    "Peer public address not available for hole punching");
        state_ = ConnectionState::FAILED;
        callback(std::make_error_code(std::errc::destination_address_required));
        return;
    }

    LOG_INFO("[P2PClient] Starting UDP hole punch to {}:{}",
             *current_peer_->public_ip, *current_peer_->public_port);

    // Resolve peer endpoint
    boost::system::error_code resolve_ec;
    auto addr = boost::asio::ip::make_address(*current_peer_->public_ip, resolve_ec);
    if (resolve_ec) {
        LOG_ERROR("[P2PClient] Invalid peer address: {}", resolve_ec.message());
        set_failure(ConnectionFailureReason::DIRECT_PATH_UNAVAILABLE,
                    "Invalid peer address: " + resolve_ec.message());
        state_ = ConnectionState::FAILED;
        callback(std::error_code(resolve_ec.value(), std::system_category()));
        return;
    }

    transport::udp::endpoint peer_ep(addr, *current_peer_->public_port);
    udp_transport_->set_peer(peer_ep);

    // Build a punch packet (HANDSHAKE message)
    protocol::HandshakeMessage punch_msg(did_, current_peer_->did);
    auto punch_data = punch_msg.encode();

    // Send multiple punch packets to open the NAT pinhole
    static constexpr int kPunchAttempts = 5;
    static constexpr auto kPunchInterval = std::chrono::milliseconds(200);

    auto attempts = std::make_shared<int>(0);
    auto punch_timer = std::make_shared<boost::asio::steady_timer>(io_context_);
    auto timeout_timer = std::make_shared<boost::asio::steady_timer>(io_context_);
    auto completed = std::make_shared<std::atomic<bool>>(false);
    auto weak_self = weak_from_this();

    // Store the punch completion callback so handle_received_message can trigger it
    punch_callback_ = [weak_self, completed, callback, timeout_timer, punch_timer]() {
        if (completed->exchange(true)) return;
        auto self = weak_self.lock();
        if (!self) return;

        timeout_timer->cancel();
        punch_timer->cancel();
        self->punch_callback_ = nullptr;

        self->active_path_ = ConnectionPath::DIRECT_P2P;
        self->last_failure_reason_ = ConnectionFailureReason::NONE;
        self->last_failure_detail_.clear();
        self->state_ = ConnectionState::CONNECTED_P2P;
        self->start_keepalive();

        LOG_INFO("[P2PClient] UDP hole punch succeeded — direct P2P established");
        if (self->on_connected_) {
            self->on_connected_();
        }
        callback(std::error_code());
    };

    // Punch timeout
    timeout_timer->expires_after(config_.punch_timeout);
    timeout_timer->async_wait([weak_self, completed, callback](const boost::system::error_code& ec) {
        if (ec || completed->exchange(true)) return;
        auto self = weak_self.lock();
        if (!self) return;

        self->punch_callback_ = nullptr;
        LOG_WARN("[P2PClient] UDP hole punch timed out");
        self->set_failure(ConnectionFailureReason::DIRECT_PATH_UNAVAILABLE,
                    "UDP hole punch timed out");
        self->state_ = ConnectionState::FAILED;
        callback(std::make_error_code(std::errc::timed_out));
    });

    // Send punch packets at intervals
    auto send_punch = std::make_shared<std::function<void()>>();
    *send_punch = [weak_self, punch_data, peer_ep, attempts, punch_timer,
                   completed, send_punch]() {
        if (completed->load()) return;
        auto self = weak_self.lock();
        if (!self) return;

        if (*attempts >= kPunchAttempts) return;
        ++(*attempts);

        LOG_DEBUG("[P2PClient] Sending punch packet {}/{}", *attempts, kPunchAttempts);
        self->udp_transport_->send_to(punch_data, peer_ep, [](const std::error_code&) {});

        punch_timer->expires_after(kPunchInterval);
        punch_timer->async_wait([completed, send_punch](const boost::system::error_code& ec) {
            if (!ec && !completed->load()) {
                (*send_punch)();
            }
        });
    };

    (*send_punch)();
}

void P2PClient::handle_received_message(const std::vector<uint8_t>& data) {
    auto msg = protocol::Message::decode(data);
    if (!msg) {
        return;
    }

    switch (msg->type()) {
        case protocol::MessageType::HANDSHAKE:
        case protocol::MessageType::HANDSHAKE_ACK:
            if (punch_callback_) {
                punch_callback_();
            }
            break;

        case protocol::MessageType::KEEPALIVE:
            // Keepalive received, connection is alive
            break;

        case protocol::MessageType::CHANNEL_DATA:
            if (msg->channel_id().has_value()) {
                int channel_id = msg->channel_id().value();

                // Store in channel queue
                if (channels_.find(channel_id) != channels_.end()) {
                    channels_[channel_id].push(msg->payload());
                }

                // Notify callback
                if (on_data_) {
                    on_data_(channel_id, msg->payload());
                }
            }
            break;

        case protocol::MessageType::DISCONNECT:
            close();
            break;

        default:
            break;
    }
}

} // namespace core
} // namespace p2p
