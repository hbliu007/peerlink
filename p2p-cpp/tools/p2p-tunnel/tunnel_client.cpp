/**
 * @file tunnel_client.cpp
 * @brief P2P tunnel client - forwards local TCP port to remote via P2P
 *
 * Usage:
 *   ./p2p-tunnel-client <my_did> <server_did> <local_port> <remote_port> [signaling_server] [stun_server]
 *
 * Example:
 *   ./p2p-tunnel-client mobile-001 office-001 9000 8080 ws://your-server.com:8080 your-server.com:3478
 */

#include "p2p/core/p2p_client.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace p2p;
using boost::asio::ip::tcp;

class TunnelClient {
public:
    TunnelClient(boost::asio::io_context& io_context,
                 const std::string& did,
                 const std::string& server_did,
                 const core::P2PConfig& config,
                 uint16_t local_port,
                 uint16_t /* remote_port */)
        : io_context_(io_context)
        , did_(did)
        , server_did_(server_did)
        , local_port_(local_port)
        , acceptor_(io_context)
        , p2p_client_(std::make_shared<core::P2PClient>(io_context, did, config))
        , channel_id_(-1)
    {}

    void start() {
        // Setup P2P event handlers
        p2p_client_->on_connected([this]() {
            std::cout << "P2P connected to " << server_did_ << std::endl;
            std::cout << "Connection type: "
                      << core::P2PClient::ToString(p2p_client_->active_path()) << std::endl;
            std::cout << "Relay fallback used: "
                      << (p2p_client_->used_relay_fallback() ? "yes" : "no") << std::endl;

            // Create P2P channel
            channel_id_ = p2p_client_->create_channel();
            std::cout << "Created P2P channel: " << channel_id_ << std::endl;

            // Start local TCP listener
            start_tcp_listener();
        });

        p2p_client_->on_disconnected([this]() {
            std::cerr << "P2P disconnected" << std::endl;
            io_context_.stop();
        });

        p2p_client_->on_data([this](int channel_id, const std::vector<uint8_t>& data) {
            if (channel_id == channel_id_ && tcp_socket_ && tcp_socket_->is_open()) {
                // Forward P2P data to TCP socket
                boost::asio::async_write(*tcp_socket_, boost::asio::buffer(data),
                    [this](const boost::system::error_code& ec, std::size_t) {
                        if (ec) {
                            std::cerr << "TCP write error: " << ec.message() << std::endl;
                            tcp_socket_->close();
                        }
                    });
            }
        });

        p2p_client_->on_error([](const std::error_code& ec, const std::string& message) {
            std::cerr << "P2P error: " << message << " (" << ec.message() << ")" << std::endl;
        });

        // Initialize P2P client
        std::cout << "Initializing P2P client..." << std::endl;
        p2p_client_->initialize([this](const std::error_code& ec) {
            if (ec) {
                std::cerr << "P2P initialization failed: " << ec.message() << std::endl;
                std::cerr << "Failure reason: "
                          << core::P2PClient::ToString(p2p_client_->last_failure_reason())
                          << " — " << p2p_client_->last_failure_detail() << std::endl;
                io_context_.stop();
                return;
            }

            std::cout << "P2P initialized, connecting to " << server_did_ << "..." << std::endl;
            p2p_client_->connect(server_did_, [this](const std::error_code& ec) {
                if (ec) {
                    std::cerr << "P2P connection failed: " << ec.message() << std::endl;
                    std::cerr << "Failure reason: "
                              << core::P2PClient::ToString(p2p_client_->last_failure_reason())
                              << " — " << p2p_client_->last_failure_detail() << std::endl;
                    io_context_.stop();
                }
            });
        });
    }

private:
    void start_tcp_listener() {
        try {
            tcp::endpoint endpoint(tcp::v4(), local_port_);
            acceptor_.open(endpoint.protocol());
            acceptor_.set_option(tcp::acceptor::reuse_address(true));
            acceptor_.bind(endpoint);
            acceptor_.listen();

            std::cout << "TCP listener started on 127.0.0.1:" << local_port_ << std::endl;
            accept_connection();
        } catch (const std::exception& e) {
            std::cerr << "Failed to start TCP listener: " << e.what() << std::endl;
            io_context_.stop();
        }
    }

    void accept_connection() {
        acceptor_.async_accept([this](const boost::system::error_code& ec, tcp::socket socket) {
            if (ec) {
                std::cerr << "Accept error: " << ec.message() << std::endl;
                return;
            }

            std::cout << "TCP connection accepted from "
                      << socket.remote_endpoint().address().to_string() << ":"
                      << socket.remote_endpoint().port() << std::endl;

            // Close previous connection if exists
            if (tcp_socket_ && tcp_socket_->is_open()) {
                tcp_socket_->close();
            }

            tcp_socket_ = std::make_shared<tcp::socket>(std::move(socket));
            read_from_tcp();

            // Accept next connection
            accept_connection();
        });
    }

    void read_from_tcp() {
        auto buffer = std::make_shared<std::vector<uint8_t>>(8192);
        tcp_socket_->async_read_some(boost::asio::buffer(*buffer),
            [this, buffer](const boost::system::error_code& ec, std::size_t bytes_transferred) {
                if (ec) {
                    if (ec != boost::asio::error::eof) {
                        std::cerr << "TCP read error: " << ec.message() << std::endl;
                    }
                    tcp_socket_->close();
                    return;
                }

                // Forward TCP data to P2P channel
                buffer->resize(bytes_transferred);
                p2p_client_->send_data(channel_id_, *buffer,
                    [this](const std::error_code& ec) {
                        if (ec) {
                            std::cerr << "P2P send error: " << ec.message() << std::endl;
                            tcp_socket_->close();
                        }
                    });

                // Continue reading
                read_from_tcp();
            });
    }

    boost::asio::io_context& io_context_;
    std::string did_;
    std::string server_did_;
    uint16_t local_port_;
    tcp::acceptor acceptor_;
    std::shared_ptr<tcp::socket> tcp_socket_;
    std::shared_ptr<core::P2PClient> p2p_client_;
    int channel_id_;
};

int main(int argc, char* argv[]) {
    if (argc < 5) {
        std::cerr << "Usage: " << argv[0] << " <my_did> <server_did> <local_port> <remote_port> [options]" << std::endl;
        std::cerr << "Options:" << std::endl;
        std::cerr << "  --signaling HOST:PORT   Signaling server (default: ws://localhost:8080)" << std::endl;
        std::cerr << "  --stun HOST:PORT        STUN server (default: stun.l.google.com:19302)" << std::endl;
        std::cerr << "  --relay-server HOST:PORT  TCP relay server for firewall traversal" << std::endl;
        std::cerr << "  --relay-mode MODE       auto|relay-preferred|relay-only|direct-only (default: auto)" << std::endl;
        std::cerr << "\nExample (relay mode from home to office):" << std::endl;
        std::cerr << "  " << argv[0] << " home-001 office-001 9000 8080 --relay-server <JUMP_HOST>:9700 --relay-mode relay-only" << std::endl;
        return 1;
    }

    try {
        std::string did = argv[1];
        std::string server_did = argv[2];
        uint16_t local_port = std::stoi(argv[3]);
        uint16_t remote_port = std::stoi(argv[4]);

        // Defaults
        std::string signaling_host = "localhost";
        uint16_t signaling_port = 8080;
        std::string stun_host = "stun.l.google.com";
        uint16_t stun_port = 19302;
        std::string relay_host;
        uint16_t relay_port = 9700;
        core::RelayMode relay_mode = core::RelayMode::AUTO;

        // Parse optional arguments
        for (int i = 5; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--signaling" && i + 1 < argc) {
                std::string val = argv[++i];
                if (val.find("ws://") == 0) val = val.substr(5);
                else if (val.find("wss://") == 0) val = val.substr(6);
                auto pos = val.find_last_of(':');
                signaling_host = val.substr(0, pos);
                signaling_port = std::stoi(val.substr(pos + 1));
            } else if (arg == "--stun" && i + 1 < argc) {
                std::string val = argv[++i];
                auto pos = val.find_last_of(':');
                stun_host = val.substr(0, pos);
                stun_port = std::stoi(val.substr(pos + 1));
            } else if (arg == "--relay-server" && i + 1 < argc) {
                std::string val = argv[++i];
                auto pos = val.find_last_of(':');
                relay_host = val.substr(0, pos);
                relay_port = std::stoi(val.substr(pos + 1));
            } else if (arg == "--relay-mode" && i + 1 < argc) {
                std::string mode = argv[++i];
                if (mode == "relay-preferred") relay_mode = core::RelayMode::RELAY_PREFERRED;
                else if (mode == "relay-only") relay_mode = core::RelayMode::RELAY_ONLY;
                else if (mode == "direct-only") relay_mode = core::RelayMode::DIRECT_ONLY;
                else relay_mode = core::RelayMode::AUTO;
            }
        }

        core::P2PConfig config;
        config.signaling_server = signaling_host;
        config.signaling_port = signaling_port;
        config.stun_server = stun_host;
        config.stun_port = stun_port;
        config.tcp_relay_server = relay_host;
        config.tcp_relay_port = relay_port;
        config.relay_mode = relay_mode;

        boost::asio::io_context io_context;
        TunnelClient client(io_context, did, server_did, config, local_port, remote_port);

        std::cout << "Starting P2P tunnel client..." << std::endl;
        std::cout << "  DID: " << did << std::endl;
        std::cout << "  Server DID: " << server_did << std::endl;
        std::cout << "  Local port: " << local_port << std::endl;
        std::cout << "  Relay mode: " << core::P2PClient::ToString(relay_mode) << std::endl;
        if (!relay_host.empty()) {
            std::cout << "  Relay server: " << relay_host << ":" << relay_port << std::endl;
        }

        client.start();
        io_context.run();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

