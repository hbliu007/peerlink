#pragma once

#include <boost/asio.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <system_error>
#include <deque>
#include <mutex>
#include <atomic>
#include "p2p/transport/message_framer.hpp"

namespace p2p {
namespace transport {

using boost::asio::ip::tcp;

/**
 * @brief Callback for received data from relay
 * @param data Received data
 */
using RelayReceiveCallback = std::function<void(const std::vector<uint8_t>& data)>;

/**
 * @brief Callback for relay events
 */
using RelayEventCallback = std::function<void(const std::string& event, const std::string& detail)>;

/**
 * @brief TCP Relay transport for firewall traversal
 *
 * Connects to a lightweight TCP relay server (simple_relay_server),
 * registers with a DID, and relays data to/from a target peer.
 *
 * Protocol:
 *   1. Connect TCP to relay server
 *   2. Send "REGISTER <did>\n" — server replies "OK\n"
 *   3. Send "CONNECT <target_did>\n" — server replies "OK CONNECTED <did>\n"
 *   4. After CONNECT, all data is relayed bidirectionally (raw bytes)
 *
 * This transport is the last-resort fallback when STUN/punch fail,
 * especially behind enterprise firewalls that block all UDP.
 */
class RelayTransport : public std::enable_shared_from_this<RelayTransport> {
public:
    enum class State {
        DISCONNECTED,
        CONNECTING,      // TCP connecting to relay server
        REGISTERING,     // Sent REGISTER, waiting OK
        REGISTERED,      // Registered, ready to CONNECT
        RELAY_CONNECTING, // Sent CONNECT, waiting OK
        RELAYING,        // Bidirectional relay active
        FAILED
    };

    using SendCallback = std::function<void(const std::error_code& ec)>;

    /**
     * @brief Construct relay transport
     * @param io_context Boost.Asio IO context
     * @param relay_host Relay server hostname/IP
     * @param relay_port Relay server port
     * @param did This device's DID
     */
    RelayTransport(boost::asio::io_context& io_context,
                   const std::string& relay_host,
                   uint16_t relay_port,
                   const std::string& did);

    ~RelayTransport();

    /**
     * @brief Start: connect to relay and register DID
     * @param callback Called when registration completes (or fails)
     */
    void start(std::function<void(const std::error_code&)> callback);

    /**
     * @brief Stop the transport and close the socket
     */
    void stop();

    /**
     * @brief Connect to a target peer via relay
     * @param target_did Target device DID
     * @param callback Called when relay connection established (or fails)
     */
    void connect_to_peer(const std::string& target_did,
                         std::function<void(const std::error_code&)> callback);

    /**
     * @brief Send data through the relay to the connected peer
     * @param data Data to send
     * @param callback Completion callback
     */
    void send(const std::vector<uint8_t>& data, SendCallback callback);

    /**
     * @brief Set callback for received data
     */
    void set_receive_callback(RelayReceiveCallback callback);

    /**
     * @brief Set callback for relay events (connected, disconnected, etc.)
     */
    void set_event_callback(RelayEventCallback callback);

    /**
     * @brief Check if relay is active (in RELAYING state)
     */
    bool is_relaying() const { return state_ == State::RELAYING; }

    /**
     * @brief Check if registered with relay server
     */
    bool is_registered() const { return state_ == State::REGISTERED || is_relaying(); }

    /**
     * @brief Get current state
     */
    State state() const { return state_; }

    /**
     * @brief Check if transport is running (connected to relay)
     */
    bool is_running() const { return state_ != State::DISCONNECTED && state_ != State::FAILED; }

private:
    void do_connect(std::function<void(const std::error_code&)> callback);
    void do_register(std::function<void(const std::error_code&)> callback);
    void read_response(std::function<void(const std::string& line, const std::error_code&)> callback);
    void start_relay_read();
    void do_relay_read();
    void start_incoming_listen();
    void do_write();
    void handle_disconnect(const boost::system::error_code& ec);

    boost::asio::io_context& io_context_;
    tcp::socket socket_;
    tcp::resolver resolver_;

    std::string relay_host_;
    uint16_t relay_port_;
    std::string did_;
    std::atomic<State> state_;

    boost::asio::streambuf response_buffer_;

    RelayReceiveCallback receive_callback_;
    RelayEventCallback event_callback_;

    MessageFramer framer_;

    std::mutex write_mutex_;
    std::deque<std::pair<std::shared_ptr<std::vector<uint8_t>>, SendCallback>> write_queue_;
};

} // namespace transport
} // namespace p2p
