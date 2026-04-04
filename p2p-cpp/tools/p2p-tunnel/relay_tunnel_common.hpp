/**
 * @file relay_tunnel_common.hpp
 * @brief Common classes for relay tunnel implementations
 *
 * Shared by both single-session and multi-session relay tunnel tools.
 * Contains RelayConnection and TcpBridge classes.
 */

#pragma once

#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <functional>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

// ─── Relay Connection ───────────────────────────────────────────────
// Manages connection to the relay server, handles REGISTER/CONNECT protocol,
// then switches to raw byte forwarding.
class RelayConnection : public std::enable_shared_from_this<RelayConnection> {
public:
    using DataCallback = std::function<void(const std::vector<uint8_t>&)>;
    using EventCallback = std::function<void(const std::string&)>;

    RelayConnection(asio::io_context& io, const std::string& relay_host, uint16_t relay_port)
        : io_(io), resolver_(io), socket_(io)
        , relay_host_(relay_host), relay_port_(relay_port)
    {}

    void set_data_callback(DataCallback cb) { data_cb_ = std::move(cb); }
    void set_event_callback(EventCallback cb) { event_cb_ = std::move(cb); }

    void connect_and_register(const std::string& did, std::function<void(bool)> on_registered) {
        did_ = did;
        auto self = shared_from_this();
        resolver_.async_resolve(relay_host_, std::to_string(relay_port_),
            [this, self, on_registered](const boost::system::error_code& ec,
                                         tcp::resolver::results_type results) {
                if (ec) {
                    std::cerr << "[Relay] Resolve failed: " << ec.message() << std::endl;
                    on_registered(false);
                    return;
                }
                asio::async_connect(socket_, results,
                    [this, self, on_registered](const boost::system::error_code& ec, const tcp::endpoint&) {
                        if (ec) {
                            std::cerr << "[Relay] Connect failed: " << ec.message() << std::endl;
                            on_registered(false);
                            return;
                        }
                        std::cout << "[Relay] Connected to " << relay_host_ << ":" << relay_port_ << std::endl;

                        // Send REGISTER
                        auto cmd = std::make_shared<std::string>("REGISTER " + did_ + "\n");
                        asio::async_write(socket_, asio::buffer(*cmd),
                            [this, self, cmd, on_registered](const boost::system::error_code& ec, std::size_t) {
                                if (ec) {
                                    std::cerr << "[Relay] Register write failed: " << ec.message() << std::endl;
                                    on_registered(false);
                                    return;
                                }
                                // Read response
                                asio::async_read_until(socket_, resp_buf_, '\n',
                                    [this, self, on_registered](const boost::system::error_code& ec, std::size_t) {
                                        if (ec) {
                                            std::cerr << "[Relay] Register read failed: " << ec.message() << std::endl;
                                            on_registered(false);
                                            return;
                                        }
                                        std::istream is(&resp_buf_);
                                        std::string line;
                                        std::getline(is, line);
                                        if (!line.empty() && line.back() == '\r') line.pop_back();

                                        if (line == "OK") {
                                            std::cout << "[Relay] Registered as: " << did_ << std::endl;
                                            on_registered(true);
                                        } else {
                                            std::cerr << "[Relay] Register failed: " << line << std::endl;
                                            on_registered(false);
                                        }
                                    });
                            });
                    });
            });
    }

    void connect_to_peer(const std::string& target_did, std::function<void(bool)> on_connected) {
        auto self = shared_from_this();
        auto cmd = std::make_shared<std::string>("CONNECT " + target_did + "\n");
        asio::async_write(socket_, asio::buffer(*cmd),
            [this, self, cmd, on_connected](const boost::system::error_code& ec, std::size_t) {
                if (ec) {
                    std::cerr << "[Relay] Connect write failed: " << ec.message() << std::endl;
                    on_connected(false);
                    return;
                }
                // Read response
                asio::async_read_until(socket_, resp_buf_, '\n',
                    [this, self, on_connected](const boost::system::error_code& ec, std::size_t) {
                        if (ec) {
                            std::cerr << "[Relay] Connect read failed: " << ec.message() << std::endl;
                            on_connected(false);
                            return;
                        }
                        std::istream is(&resp_buf_);
                        std::string line;
                        std::getline(is, line);
                        if (!line.empty() && line.back() == '\r') line.pop_back();

                        if (line.find("OK") == 0) {
                            // Relay responds "OK" or "OK CONNECTED" — both mean success
                            std::cout << "[Relay] " << line << std::endl;
                            on_connected(true);
                        } else {
                            std::cerr << "[Relay] Connect failed: " << line << std::endl;
                            on_connected(false);
                        }
                    });
            });
    }

    // Wait for incoming CONNECT from peer (server mode)
    void wait_for_connection(std::function<void(bool, const std::string&)> on_connected) {
        auto self = shared_from_this();
        asio::async_read_until(socket_, resp_buf_, '\n',
            [this, self, on_connected](const boost::system::error_code& ec, std::size_t) {
                if (ec) {
                    std::cerr << "[Relay] Wait read failed: " << ec.message() << std::endl;
                    on_connected(false, "");
                    return;
                }
                std::istream is(&resp_buf_);
                std::string line;
                std::getline(is, line);
                if (!line.empty() && line.back() == '\r') line.pop_back();

                if (line.find("OK CONNECTED") == 0) {
                    std::string peer = line.substr(13);
                    std::cout << "[Relay] Peer connected: " << peer << std::endl;
                    on_connected(true, peer);
                } else if (line.find("INCOMING") == 0) {
                    // Relay sends "INCOMING <peer_did>" when a client connects
                    std::string peer = line.substr(9);
                    std::cout << "[Relay] Peer connected (INCOMING): " << peer << std::endl;
                    on_connected(true, peer);
                } else {
                    std::cerr << "[Relay] Unexpected: " << line << std::endl;
                    on_connected(false, "");
                }
            });
    }

    // Start raw data relay
    void start_relay_read() {
        auto self = shared_from_this();

        // Forward any leftover data in resp_buf_
        if (resp_buf_.size() > 0) {
            std::vector<uint8_t> leftover(resp_buf_.size());
            asio::buffer_copy(asio::buffer(leftover), resp_buf_.data());
            resp_buf_.consume(resp_buf_.size());
            if (data_cb_) data_cb_(leftover);
        }

        do_relay_read();
    }

    void send(const std::vector<uint8_t>& data) {
        auto self = shared_from_this();
        auto buf = std::make_shared<std::vector<uint8_t>>(data);
        std::cout << "[Relay] Queuing " << data.size() << " bytes to send" << std::endl;

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

    void close() {
        boost::system::error_code ec;
        socket_.close(ec);
    }

    asio::io_context& get_io_context() {
        return io_;
    }

private:
    void do_relay_read() {
        auto self = shared_from_this();
        auto buf = std::make_shared<std::vector<uint8_t>>(8192);
        socket_.async_read_some(asio::buffer(*buf),
            [this, self, buf](const boost::system::error_code& ec, std::size_t bytes) {
                if (ec) {
                    if (ec != asio::error::eof && ec != asio::error::operation_aborted) {
                        std::cerr << "[Relay] Read error: " << ec.message() << std::endl;
                    }
                    if (event_cb_) event_cb_("disconnected");
                    return;
                }
                std::cout << "[Relay] Read " << bytes << " bytes from relay" << std::endl;
                if (data_cb_) {
                    std::vector<uint8_t> data(buf->begin(), buf->begin() + bytes);
                    data_cb_(data);
                }
                do_relay_read();
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

        std::cout << "[Relay] Writing " << buf->size() << " bytes to socket" << std::endl;
        asio::async_write(socket_, asio::buffer(*buf),
            [this, self, buf](const boost::system::error_code& ec, std::size_t bytes) {
                if (ec) {
                    std::cerr << "[Relay] Write error: " << ec.message() << std::endl;
                    if (event_cb_) event_cb_("disconnected");
                    return;
                }
                std::cout << "[Relay] Wrote " << bytes << " bytes successfully" << std::endl;
                std::lock_guard<std::mutex> lock(write_mutex_);
                write_queue_.pop_front();
                if (!write_queue_.empty()) {
                    asio::post(io_, [this, self]() { do_write(); });
                }
            });
    }

    asio::io_context& io_;
    tcp::resolver resolver_;
    tcp::socket socket_;
    std::string relay_host_;
    uint16_t relay_port_;
    std::string did_;
    asio::streambuf resp_buf_;

    DataCallback data_cb_;
    EventCallback event_cb_;

    std::mutex write_mutex_;
    std::deque<std::shared_ptr<std::vector<uint8_t>>> write_queue_;
};

// ─── TCP Bridge ─────────────────────────────────────────────────────
// Bridges a local TCP connection to the relay connection.
class TcpBridge : public std::enable_shared_from_this<TcpBridge> {
public:
    TcpBridge(tcp::socket socket, std::shared_ptr<RelayConnection> relay)
        : socket_(std::move(socket)), relay_(relay)
    {}

    void start() {
        read_from_tcp();
    }

    void send_to_tcp(const std::vector<uint8_t>& data) {
        auto self = shared_from_this();
        auto buf = std::make_shared<std::vector<uint8_t>>(data);
        std::cout << "[TCP] Sending " << data.size() << " bytes to TCP" << std::endl;
        asio::async_write(socket_, asio::buffer(*buf),
            [self, buf](const boost::system::error_code& ec, std::size_t bytes) {
                if (ec) {
                    std::cerr << "[TCP] Write error: " << ec.message() << std::endl;
                } else {
                    std::cout << "[TCP] Wrote " << bytes << " bytes" << std::endl;
                }
            });
    }

    void close() {
        boost::system::error_code ec;
        socket_.close(ec);
    }

private:
    void read_from_tcp() {
        auto self = shared_from_this();
        auto buf = std::make_shared<std::vector<uint8_t>>(8192);
        socket_.async_read_some(asio::buffer(*buf),
            [this, self, buf](const boost::system::error_code& ec, std::size_t bytes) {
                if (ec) {
                    if (ec != asio::error::eof) {
                        std::cerr << "[TCP] Read error: " << ec.message() << std::endl;
                    }
                    std::cout << "[TCP] Connection closed" << std::endl;
                    relay_->close();
                    return;
                }
                std::cout << "[TCP] Read " << bytes << " bytes from TCP" << std::endl;
                std::vector<uint8_t> data(buf->begin(), buf->begin() + bytes);
                relay_->send(data);
                read_from_tcp();
            });
    }

    tcp::socket socket_;
    std::shared_ptr<RelayConnection> relay_;
};

