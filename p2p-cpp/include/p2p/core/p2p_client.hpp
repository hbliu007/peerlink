#pragma once

#include "p2p/protocol/message.hpp"
#include "p2p/transport/udp_transport.hpp"
#include "p2p/nat/stun_client.hpp"
#include <optional>
#include <boost/asio.hpp>
#include <memory>
#include <string>
#include <functional>
#include <map>
#include <queue>
#include <atomic>

namespace p2p {

namespace transport {
class RelayTransport;  // forward declaration
}

namespace core {

/**
 * @brief Connection state machine
 */
enum class ConnectionState {
    DISCONNECTED,      // Not connected
    CONNECTING,        // Attempting connection
    HANDSHAKE,         // Exchanging handshake
    CONNECTED_P2P,     // Connected via P2P
    CONNECTED_RELAY,   // Connected via relay
    FAILED             // Connection failed
};

/**
 * @brief P2P client configuration
 */
/**
 * @brief Relay mode selection
 */
enum class RelayMode {
    AUTO,             // Try direct P2P first, fall back to relay
    RELAY_PREFERRED,  // Prefer relay, but allow direct when relay unavailable
    RELAY_ONLY,       // Skip STUN/punch, use relay directly
    DIRECT_ONLY       // Never use relay
};

enum class ConnectionPath {
    NONE,
    DIRECT_P2P,
    RELAY
};

enum class ConnectionFailureReason {
    NONE,
    NOT_RUNNING,
    ALREADY_CONNECTED,
    NAT_DETECTION_FAILED,
    DIRECT_PATH_UNAVAILABLE,
    DIRECT_PATH_NOT_IMPLEMENTED,
    RELAY_NOT_CONFIGURED,
    RELAY_REGISTRATION_FAILED,
    RELAY_CONNECT_FAILED,
    NO_TRANSPORT_AVAILABLE
};

struct P2PConfig {
    std::string signaling_server = "localhost";
    uint16_t signaling_port = 8443;
    std::string stun_server = "stun.l.google.com";
    uint16_t stun_port = 19302;
    std::string relay_server = "localhost";
    uint16_t relay_port = 5000;
    uint16_t local_port = 0;  // 0 for auto-assign
    std::chrono::seconds connection_timeout{30};
    std::chrono::seconds punch_timeout{10};
    std::chrono::seconds keepalive_interval{5};
    int max_retries = 3;
    bool auto_relay = true;

    // TCP Relay (lightweight relay for firewall traversal)
    std::string tcp_relay_server;   // e.g. "your-relay-server.com"
    uint16_t tcp_relay_port = 9700;
    RelayMode relay_mode = RelayMode::AUTO;
};

/**
 * @brief Peer information
 */
struct PeerInfo {
    std::string did;
    std::optional<std::string> public_ip;
    std::optional<uint16_t> public_port;
    std::optional<std::string> local_ip;
    std::optional<uint16_t> local_port;
    std::optional<nat::NATType> nat_type;
    std::vector<std::string> capabilities;
};

/**
 * @brief P2P Client for device-to-device communication
 *
 * Features:
 * - NAT type detection
 * - UDP hole punching
 * - Multi-channel data transfer
 * - Automatic relay fallback
 */
class P2PClient : public std::enable_shared_from_this<P2PClient> {
public:
    using ConnectedCallback = std::function<void()>;
    using DisconnectedCallback = std::function<void()>;
    using DataCallback = std::function<void(int channel_id, const std::vector<uint8_t>& data)>;
    using ErrorCallback = std::function<void(const std::error_code& ec, const std::string& message)>;

    /**
     * @brief Construct P2P client
     * @param io_context Boost.Asio IO context
     * @param did Device ID
     * @param config Client configuration
     */
    P2PClient(boost::asio::io_context& io_context,
              const std::string& did,
              const P2PConfig& config = P2PConfig());

    ~P2PClient();

    /**
     * @brief Initialize the client (NAT detection, signaling connection)
     * @param callback Completion callback
     */
    void initialize(std::function<void(const std::error_code&)> callback);

    /**
     * @brief Connect to a remote device
     * @param peer_did Target device ID
     * @param callback Completion callback
     */
    void connect(const std::string& peer_did,
                 std::function<void(const std::error_code&)> callback);

    /**
     * @brief Send data on a channel
     * @param channel_id Channel ID
     * @param data Data to send
     * @param callback Completion callback
     */
    void send_data(int channel_id,
                   const std::vector<uint8_t>& data,
                   std::function<void(const std::error_code&)> callback);

    /**
     * @brief Create a new data channel
     * @return Channel ID
     */
    int create_channel();

    /**
     * @brief Close a data channel
     * @param channel_id Channel ID to close
     */
    void close_channel(int channel_id);

    /**
     * @brief Close the connection
     */
    void close();

    /**
     * @brief Reset connection state for retry without disconnecting relay transport
     */
    void reset_for_retry();

    // Event callbacks
    void on_connected(ConnectedCallback callback) { on_connected_ = callback; }
    void on_disconnected(DisconnectedCallback callback) { on_disconnected_ = callback; }
    void on_data(DataCallback callback) { on_data_ = callback; }
    void on_error(ErrorCallback callback) { on_error_ = callback; }

    // Getters
    ConnectionState state() const { return state_; }
    bool is_connected() const {
        return state_ == ConnectionState::CONNECTED_P2P ||
               state_ == ConnectionState::CONNECTED_RELAY;
    }
    bool is_p2p() const { return state_ == ConnectionState::CONNECTED_P2P; }
    ConnectionPath active_path() const { return active_path_; }
    ConnectionFailureReason last_failure_reason() const { return last_failure_reason_; }
    const std::string& last_failure_detail() const { return last_failure_detail_; }
    bool used_relay_fallback() const { return used_relay_fallback_; }
    const std::string& did() const { return did_; }
    std::optional<PeerInfo> peer() const { return current_peer_; }

    static const char* ToString(RelayMode mode);
    static const char* ToString(ConnectionPath path);
    static const char* ToString(ConnectionFailureReason reason);

private:
    void detect_nat(std::function<void(const std::error_code&)> callback);
    void init_relay_transport(std::function<void(const std::error_code&)> callback);
    void set_failure(ConnectionFailureReason reason, const std::string& detail);
    bool has_direct_path() const;
    bool has_registered_relay() const;
    void attempt_relay_connect(
        const std::string& peer_did,
        bool fallback,
        std::function<void(const std::error_code&)> callback);
    void complete_direct_connect(std::function<void(const std::error_code&)> callback);
    void start_keepalive();
    void send_keepalive();
    void handle_received_message(const std::vector<uint8_t>& data);

    boost::asio::io_context& io_context_;
    std::string did_;
    P2PConfig config_;

    std::atomic<ConnectionState> state_;
    std::optional<nat::NATType> nat_type_;
    std::optional<std::pair<std::string, uint16_t>> public_addr_;
    std::optional<PeerInfo> current_peer_;

    std::shared_ptr<transport::UDPTransport> udp_transport_;
    std::shared_ptr<transport::RelayTransport> relay_transport_;
    ConnectionPath active_path_;
    ConnectionFailureReason last_failure_reason_;
    std::string last_failure_detail_;
    bool used_relay_fallback_;

    std::map<int, std::queue<std::vector<uint8_t>>> channels_;
    int next_channel_id_;

    boost::asio::steady_timer keepalive_timer_;
    std::function<void()> punch_callback_;
    bool running_;
    bool closing_;

    ConnectedCallback on_connected_;
    DisconnectedCallback on_disconnected_;
    DataCallback on_data_;
    ErrorCallback on_error_;
};

} // namespace core
} // namespace p2p
