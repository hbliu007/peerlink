/**
 * @file simple_relay_server.cpp
 * @brief Lightweight TCP Relay Server for enterprise firewall traversal
 *
 * Protocol:
 *   1. Client connects via TCP
 *   2. Client sends: "REGISTER <did>\n" to register its device ID
 *   3. Client sends: "CONNECT <target_did>\n" to request relay to target
 *   4. Server replies: "OK\n" on success, "ERROR <msg>\n" on failure
 *   5. After CONNECT succeeds, all subsequent data is relayed bidirectionally
 *
 * Usage:
 *   ./simple-relay-server [--port PORT] [--host HOST]
 *
 * Deploy on jump server (<JUMP_HOST>) where both office and home can reach.
 */

#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <csignal>
#include <atomic>
#include <deque>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

class RelaySession;

// Global registry: DID -> session
class SessionRegistry {
public:
    void register_session(const std::string& did, std::shared_ptr<RelaySession> session) {
        std::lock_guard<std::mutex> lock(mutex_);
        sessions_[did] = session;
        std::cout << "[Registry] Registered: " << did
                  << " (total: " << sessions_.size() << ")" << std::endl;
    }

    void unregister_session(const std::string& did) {
        std::lock_guard<std::mutex> lock(mutex_);
        sessions_.erase(did);
        std::cout << "[Registry] Unregistered: " << did
                  << " (total: " << sessions_.size() << ")" << std::endl;
    }

    std::shared_ptr<RelaySession> find_session(const std::string& did) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(did);
        if (it != sessions_.end()) {
            return it->second;
        }
        return nullptr;
    }

    size_t size() {
        std::lock_guard<std::mutex> lock(mutex_);
        return sessions_.size();
    }

    void list_sessions() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "[Registry] Active sessions (" << sessions_.size() << "):" << std::endl;
        for (const auto& [did, _] : sessions_) {
            std::cout << "  - " << did << std::endl;
        }
    }

private:
    std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<RelaySession>> sessions_;
};

// Global registry instance
SessionRegistry g_registry;

class RelaySession : public std::enable_shared_from_this<RelaySession> {
public:
    RelaySession(tcp::socket socket)
        : socket_(std::move(socket))
        , relay_active_(false)
    {
        auto ep = socket_.remote_endpoint();
        remote_addr_ = ep.address().to_string() + ":" + std::to_string(ep.port());
    }

    ~RelaySession() {
        if (!did_.empty()) {
            g_registry.unregister_session(did_);
        }
    }

    void start() {
        std::cout << "[Session] New connection from " << remote_addr_ << std::endl;
        read_command();
    }

    // Called by peer to push data to this session's client
    void relay_data(const std::vector<uint8_t>& data) {
        auto self = shared_from_this();
        auto buf = std::make_shared<std::vector<uint8_t>>(data);

        bool write_in_progress;
        {
            std::lock_guard<std::mutex> lock(write_mutex_);
            write_in_progress = !write_queue_.empty();
            write_queue_.push_back(buf);
        }

        if (!write_in_progress) {
            do_write();
        }
    }

    // Set the peer this session is relaying to
    void set_relay_peer(std::shared_ptr<RelaySession> peer) {
        relay_peer_ = peer;
        relay_active_ = true;
        // Cancel any pending read_command() so relay reads don't compete
        boost::system::error_code ec;
        socket_.cancel(ec);
    }

    const std::string& did() const { return did_; }
    bool is_relay_active() const { return relay_active_; }

private:
    void read_command() {
        auto self = shared_from_this();
        asio::async_read_until(socket_, cmd_buffer_, '\n',
            [this, self](const boost::system::error_code& ec, std::size_t bytes) {
                if (ec) {
                    if (ec == asio::error::operation_aborted && relay_active_) {
                        // Canceled by set_relay_peer() — relay reads take over.
                        // Forward any leftover data in cmd_buffer_ to peer.
                        if (cmd_buffer_.size() > 0) {
                            auto peer = relay_peer_.lock();
                            if (peer) {
                                std::vector<uint8_t> leftover(cmd_buffer_.size());
                                asio::buffer_copy(asio::buffer(leftover),
                                                  cmd_buffer_.data());
                                cmd_buffer_.consume(cmd_buffer_.size());
                                peer->relay_data(leftover);
                            }
                        }
                        return;
                    }
                    handle_disconnect(ec);
                    return;
                }

                std::istream is(&cmd_buffer_);
                std::string line;
                std::getline(is, line);
                // Remove \r if present
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }

                handle_command(line);
            });
    }

    void handle_command(const std::string& line) {
        std::cout << "[Session " << remote_addr_ << "] Command: " << line << std::endl;

        if (line.substr(0, 9) == "REGISTER ") {
            std::string did = line.substr(9);
            if (did.empty()) {
                send_response("ERROR empty DID\n");
                read_command();
                return;
            }

            // Unregister old DID if re-registering
            if (!did_.empty()) {
                g_registry.unregister_session(did_);
            }

            did_ = did;
            g_registry.register_session(did_, shared_from_this());
            send_response("OK\n");
            read_command();

        } else if (line.substr(0, 8) == "CONNECT ") {
            std::string target_did = line.substr(8);
            if (target_did.empty()) {
                send_response("ERROR empty target DID\n");
                read_command();
                return;
            }

            if (did_.empty()) {
                send_response("ERROR register first\n");
                read_command();
                return;
            }

            auto peer = g_registry.find_session(target_did);
            if (!peer) {
                send_response("ERROR target not found: " + target_did + "\n");
                read_command();
                return;
            }

            if (peer->is_relay_active()) {
                send_response("ERROR target already connected\n");
                read_command();
                return;
            }

            // Establish bidirectional relay
            relay_peer_ = peer;
            peer->set_relay_peer(shared_from_this());
            relay_active_ = true;

            std::cout << "[Relay] Established: " << did_ << " <-> " << target_did << std::endl;

            // Send OK to both sides
            send_response("OK CONNECTED " + target_did + "\n");
            peer->send_response("OK CONNECTED " + did_ + "\n");

            // Start relay mode - read raw data and forward
            start_relay_read();
            peer->start_relay_read();

        } else if (line == "LIST") {
            g_registry.list_sessions();
            send_response("OK\n");
            read_command();

        } else if (line == "PING") {
            send_response("PONG\n");
            read_command();

        } else {
            send_response("ERROR unknown command\n");
            read_command();
        }
    }

    void send_response(const std::string& msg) {
        auto self = shared_from_this();
        auto buf = std::make_shared<std::string>(msg);
        asio::async_write(socket_, asio::buffer(*buf),
            [self, buf](const boost::system::error_code& ec, std::size_t) {
                if (ec) {
                    std::cerr << "[Session] Write error: " << ec.message() << std::endl;
                }
            });
    }

    void start_relay_read() {
        auto self = shared_from_this();
        auto buf = std::make_shared<std::vector<uint8_t>>(8192);

        socket_.async_read_some(asio::buffer(*buf),
            [this, self, buf](const boost::system::error_code& ec, std::size_t bytes) {
                if (ec) {
                    handle_disconnect(ec);
                    return;
                }

                // Forward data to peer
                if (auto peer = relay_peer_.lock()) {
                    std::vector<uint8_t> data(buf->begin(), buf->begin() + bytes);
                    peer->relay_data(data);
                } else {
                    std::cerr << "[Session " << did_ << "] Relay peer gone" << std::endl;
                    return;
                }

                // Continue reading
                start_relay_read();
            });
    }

    void do_write() {
        auto self = shared_from_this();
        std::shared_ptr<std::vector<uint8_t>> buf;

        {
            std::lock_guard<std::mutex> lock(write_mutex_);
            if (write_queue_.empty()) return;
            buf = write_queue_.front();
        }

        asio::async_write(socket_, asio::buffer(*buf),
            [this, self, buf](const boost::system::error_code& ec, std::size_t) {
                if (ec) {
                    handle_disconnect(ec);
                    return;
                }

                std::lock_guard<std::mutex> lock(write_mutex_);
                write_queue_.pop_front();
                if (!write_queue_.empty()) {
                    // Post next write to avoid recursive calls
                    asio::post(socket_.get_executor(), [this, self]() {
                        do_write();
                    });
                }
            });
    }

    void handle_disconnect(const boost::system::error_code& ec) {
        if (ec != asio::error::eof && ec != asio::error::operation_aborted) {
            std::cerr << "[Session " << did_ << "] Error: " << ec.message() << std::endl;
        }

        std::cout << "[Session " << did_ << "] Disconnected" << std::endl;

        // Notify relay peer
        if (auto peer = relay_peer_.lock()) {
            peer->relay_active_ = false;
            peer->relay_peer_.reset();
            // Don't close peer - they may reconnect to another target
        }

        relay_active_ = false;

        if (!did_.empty()) {
            g_registry.unregister_session(did_);
            did_.clear();
        }

        // Close socket
        boost::system::error_code close_ec;
        socket_.close(close_ec);
    }

    tcp::socket socket_;
    std::string did_;
    std::string remote_addr_;
    asio::streambuf cmd_buffer_;

    bool relay_active_;
    std::weak_ptr<RelaySession> relay_peer_;

    std::mutex write_mutex_;
    std::deque<std::shared_ptr<std::vector<uint8_t>>> write_queue_;
};

class SimpleRelayServer {
public:
    SimpleRelayServer(asio::io_context& io_context, const std::string& host, uint16_t port)
        : io_context_(io_context)
        , acceptor_(io_context)
    {
        tcp::endpoint endpoint(asio::ip::make_address(host), port);
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(tcp::acceptor::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen();

        std::cout << "Simple Relay Server listening on " << host << ":" << port << std::endl;
    }

    void start() {
        accept();
    }

private:
    void accept() {
        acceptor_.async_accept([this](const boost::system::error_code& ec, tcp::socket socket) {
            if (!ec) {
                auto session = std::make_shared<RelaySession>(std::move(socket));
                session->start();
            } else {
                std::cerr << "[Server] Accept error: " << ec.message() << std::endl;
            }
            accept();
        });
    }

    asio::io_context& io_context_;
    tcp::acceptor acceptor_;
};

std::atomic<bool> g_running{true};

void signal_handler(int) {
    g_running = false;
}

int main(int argc, char* argv[]) {
    std::string host = "0.0.0.0";
    uint16_t port = 9700;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if ((arg == "--host" || arg == "-h") && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [--host HOST] [--port PORT]\n"
                      << "  --host HOST   Bind address (default: 0.0.0.0)\n"
                      << "  --port PORT   Listen port (default: 9700)\n";
            return 0;
        }
    }

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    try {
        asio::io_context io_context;
        SimpleRelayServer server(io_context, host, port);

        std::cout << "=== PeerLink Simple Relay Server ===" << std::endl;
        std::cout << "Listening on " << host << ":" << port << std::endl;
        std::cout << "Protocol: REGISTER/CONNECT/LIST/PING" << std::endl;
        std::cout << "Press Ctrl+C to stop" << std::endl;

        server.start();

        // Run with signal handling
        asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code&, int) {
            std::cout << "\nShutting down..." << std::endl;
            io_context.stop();
        });

        io_context.run();

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Server stopped." << std::endl;
    return 0;
}
