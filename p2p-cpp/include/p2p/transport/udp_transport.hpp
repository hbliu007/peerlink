#pragma once

#include <boost/asio.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <system_error>

namespace p2p {
namespace transport {

using boost::asio::ip::udp;

/**
 * @brief Callback for received data
 * @param data Received data
 * @param sender_endpoint Sender's endpoint
 */
using ReceiveCallback = std::function<void(
    const std::vector<uint8_t>& data,
    const udp::endpoint& sender_endpoint
)>;

/**
 * @brief Callback for send completion
 * @param ec Error code (empty if success)
 */
using SendCallback = std::function<void(const std::error_code& ec)>;

/**
 * @brief UDP transport for P2P communication
 *
 * Handles raw UDP socket operations for hole punching
 * and direct peer-to-peer data transfer.
 */
class UDPTransport : public std::enable_shared_from_this<UDPTransport> {
public:
    /**
     * @brief Construct UDP transport
     * @param io_context Boost.Asio IO context
     * @param local_port Local port (0 for auto-assign)
     */
    explicit UDPTransport(boost::asio::io_context& io_context, uint16_t local_port = 0);

    ~UDPTransport();

    /**
     * @brief Start the UDP transport
     * @param ec Error code output
     */
    void start(std::error_code& ec);

    /**
     * @brief Stop the UDP transport
     */
    void stop();

    /**
     * @brief Send data to specific endpoint
     * @param data Data to send
     * @param endpoint Target endpoint
     * @param callback Completion callback
     */
    void send_to(const std::vector<uint8_t>& data,
                 const udp::endpoint& endpoint,
                 SendCallback callback);

    /**
     * @brief Set peer endpoint for send operations
     * @param endpoint Peer's endpoint
     */
    void set_peer(const udp::endpoint& endpoint);

    /**
     * @brief Send data to configured peer
     * @param data Data to send
     * @param callback Completion callback
     */
    void send(const std::vector<uint8_t>& data, SendCallback callback);

    /**
     * @brief Set receive callback
     * @param callback Callback for received data
     */
    void set_receive_callback(ReceiveCallback callback);

    /**
     * @brief Get local endpoint
     * @return Local endpoint
     */
    udp::endpoint local_endpoint() const;

    /**
     * @brief Check if transport is running
     */
    bool is_running() const { return running_; }

private:
    void start_receive();
    void handle_receive(const boost::system::error_code& ec, std::size_t bytes_transferred);

    boost::asio::io_context& io_context_;
    udp::socket socket_;
    udp::endpoint peer_endpoint_;
    udp::endpoint recv_endpoint_;
    std::vector<uint8_t> recv_buffer_;
    ReceiveCallback receive_callback_;
    bool running_;
};

} // namespace transport
} // namespace p2p
