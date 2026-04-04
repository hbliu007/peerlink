#pragma once

#include "p2p/core/connection.hpp"
#include <boost/asio.hpp>
#include <atomic>
#include <mutex>
#include <memory>

namespace p2p {

/**
 * TCP-based connection implementation
 *
 * Provides async TCP socket management with proper state machine,
 * zero-copy send/receive, and RAII resource management.
 */
class TcpConnection : public Connection, public std::enable_shared_from_this<TcpConnection> {
public:
    /**
     * Create a new TCP connection
     * @param io_context Boost.Asio IO context
     * @param peer_id Remote peer ID
     * @return Shared pointer to connection
     */
    static std::shared_ptr<TcpConnection> Create(
        boost::asio::io_context& io_context,
        const PeerId& peer_id);

    /**
     * Create connection from existing socket (for accepted connections)
     * @param socket Existing connected socket
     * @param peer_id Remote peer ID
     * @return Shared pointer to connection
     */
    static std::shared_ptr<TcpConnection> Create(
        boost::asio::ip::tcp::socket socket,
        const PeerId& peer_id);

    ~TcpConnection() override;

    // Connection interface implementation
    ConnectionId GetId() const override;
    const PeerId& GetPeerId() const override;
    TransportState GetState() const override;
    Status Send(std::span<const uint8_t> data) override;
    void ReceiveAsync(ReceiveCallback callback) override;
    Status Close() override;
    Multiaddr GetLocalAddr() const override;
    Multiaddr GetRemoteAddr() const override;

    /**
     * Connect to remote endpoint
     * @param endpoint Remote TCP endpoint
     * @param callback Completion callback
     */
    void ConnectAsync(
        const boost::asio::ip::tcp::endpoint& endpoint,
        std::function<void(Status)> callback);

private:
    TcpConnection(boost::asio::io_context& io_context, const PeerId& peer_id);
    TcpConnection(boost::asio::ip::tcp::socket socket, const PeerId& peer_id);

    void StartReceive();
    void HandleReceive(const boost::system::error_code& ec, std::size_t bytes_transferred);
    void SetState(TransportState new_state);

    static std::atomic<ConnectionId> next_id_;

    ConnectionId id_;
    PeerId peer_id_;
    std::atomic<TransportState> state_;
    boost::asio::ip::tcp::socket socket_;
    std::vector<uint8_t> recv_buffer_;
    ReceiveCallback receive_callback_;
    mutable std::mutex mutex_;
};

}  // namespace p2p
