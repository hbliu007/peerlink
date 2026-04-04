#include "p2p/transport/relay_transport.hpp"
#include "p2p/utils/logger.hpp"
#include <sstream>
#include <atomic>

namespace asio = boost::asio;

namespace p2p {
namespace transport {

RelayTransport::RelayTransport(boost::asio::io_context& io_context,
                               const std::string& relay_host,
                               uint16_t relay_port,
                               const std::string& did)
    : io_context_(io_context)
    , socket_(io_context)
    , resolver_(io_context)
    , relay_host_(relay_host)
    , relay_port_(relay_port)
    , did_(did)
    , state_(State::DISCONNECTED)
{
}

RelayTransport::~RelayTransport() {
    stop();
}

void RelayTransport::start(std::function<void(const std::error_code&)> callback) {
    if (state_ != State::DISCONNECTED && state_ != State::FAILED) {
        callback(std::make_error_code(std::errc::already_connected));
        return;
    }

    state_ = State::CONNECTING;

    // Connect to relay server, then register
    do_connect([this, callback](const std::error_code& ec) {
        if (ec) {
            state_ = State::FAILED;
            callback(ec);
            return;
        }

        // TCP connected, now register DID
        do_register(callback);
    });
}

void RelayTransport::stop() {
    if (state_ == State::DISCONNECTED) {
        return;
    }

    state_ = State::DISCONNECTED;
    framer_.clear();

    boost::system::error_code ec;
    if (socket_.is_open()) {
        socket_.shutdown(tcp::socket::shutdown_both, ec);
        socket_.close(ec);
    }

    std::lock_guard<std::mutex> lock(write_mutex_);
    write_queue_.clear();
}

void RelayTransport::connect_to_peer(const std::string& target_did,
                                     std::function<void(const std::error_code&)> callback) {
    if (state_ != State::REGISTERED) {
        callback(std::make_error_code(std::errc::not_connected));
        return;
    }

    state_ = State::RELAY_CONNECTING;

    // Cancel any pending incoming listen read and clear stale buffer data
    socket_.cancel();
    response_buffer_.consume(response_buffer_.size());

    // Send CONNECT command
    std::string cmd = "CONNECT " + target_did + "\n";
    auto buf = std::make_shared<std::string>(cmd);
    auto self = shared_from_this();

    asio::async_write(socket_, asio::buffer(*buf),
        [this, self, buf, callback](const boost::system::error_code& ec, std::size_t) {
            if (ec) {
                state_ = State::FAILED;
                callback(std::error_code(ec.value(), std::system_category()));
                return;
            }

            // Read response
            read_response([this, self, callback](const std::string& line, const std::error_code& ec) {
                if (ec) {
                    state_ = State::FAILED;
                    callback(ec);
                    return;
                }

                if (line.substr(0, 2) == "OK") {
                    state_ = State::RELAYING;
                    LOG_INFO("[RelayTransport] Relay connected: {}", line);

                    if (event_callback_) {
                        event_callback_("connected", line);
                    }

                    // Start reading relay data
                    start_relay_read();
                    callback(std::error_code());
                } else if (line.substr(0, 9) == "INCOMING ") {
                    // Peer connected to us while we were trying to connect to them
                    state_ = State::RELAYING;
                    std::string peer = line.substr(9);
                    LOG_INFO("[RelayTransport] Incoming connection from {} (while connecting)", peer);

                    start_relay_read();
                    callback(std::error_code());
                } else {
                    state_ = State::REGISTERED;  // revert to registered
                    LOG_ERROR("[RelayTransport] Connect failed: {}", line);
                    // Restart incoming listen since we reverted to REGISTERED
                    start_incoming_listen();
                    callback(std::make_error_code(std::errc::connection_refused));
                }
            });
        });
}

void RelayTransport::send(const std::vector<uint8_t>& data, SendCallback callback) {
    if (state_ != State::RELAYING) {
        if (callback) {
            callback(std::make_error_code(std::errc::not_connected));
        }
        return;
    }

    auto buf = std::make_shared<std::vector<uint8_t>>(data);
    bool write_in_progress;
    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        write_in_progress = !write_queue_.empty();
        write_queue_.push_back({buf, callback});
    }

    if (!write_in_progress) {
        do_write();
    }
}

void RelayTransport::set_receive_callback(RelayReceiveCallback callback) {
    receive_callback_ = callback;
}

void RelayTransport::set_event_callback(RelayEventCallback callback) {
    event_callback_ = callback;
}

// --- Private methods ---

void RelayTransport::do_connect(std::function<void(const std::error_code&)> callback) {
    auto self = shared_from_this();
    auto timeout_timer = std::make_shared<boost::asio::steady_timer>(io_context_);
    auto completed = std::make_shared<std::atomic<bool>>(false);

    // 10 second timeout for TCP connection
    timeout_timer->expires_after(std::chrono::seconds(10));
    timeout_timer->async_wait([this, self, completed, callback](const boost::system::error_code& ec) {
        if (!ec && !completed->exchange(true)) {
            boost::system::error_code close_ec;
            socket_.close(close_ec);
            LOG_ERROR("[RelayTransport] Connect timed out");
            callback(std::make_error_code(std::errc::timed_out));
        }
    });

    resolver_.async_resolve(relay_host_, std::to_string(relay_port_),
        [this, self, callback, timeout_timer, completed](const boost::system::error_code& ec,
                               tcp::resolver::results_type results) {
            if (ec) {
                if (!completed->exchange(true)) {
                    timeout_timer->cancel();
                    LOG_ERROR("[RelayTransport] Resolve failed: {}", ec.message());
                    callback(std::error_code(ec.value(), std::system_category()));
                }
                return;
            }

            asio::async_connect(socket_, results,
                [this, self, callback, timeout_timer, completed](const boost::system::error_code& ec,
                                       const tcp::endpoint& endpoint) {
                    if (!completed->exchange(true)) {
                        timeout_timer->cancel();
                        if (ec) {
                            LOG_ERROR("[RelayTransport] Connect failed: {}", ec.message());
                            callback(std::error_code(ec.value(), std::system_category()));
                            return;
                        }

                        LOG_INFO("[RelayTransport] Connected to relay {}:{}",
                                 endpoint.address().to_string(), endpoint.port());

                        // Set TCP keepalive
                        socket_.set_option(asio::socket_base::keep_alive(true));

                        callback(std::error_code());
                    }
                });
        });
}

void RelayTransport::do_register(std::function<void(const std::error_code&)> callback) {
    state_ = State::REGISTERING;

    std::string cmd = "REGISTER " + did_ + "\n";
    auto buf = std::make_shared<std::string>(cmd);
    auto self = shared_from_this();

    asio::async_write(socket_, asio::buffer(*buf),
        [this, self, buf, callback](const boost::system::error_code& ec, std::size_t) {
            if (ec) {
                state_ = State::FAILED;
                callback(std::error_code(ec.value(), std::system_category()));
                return;
            }

            // Read response
            read_response([this, self, callback](const std::string& line, const std::error_code& ec) {
                if (ec) {
                    state_ = State::FAILED;
                    callback(ec);
                    return;
                }

                if (line == "OK") {
                    state_ = State::REGISTERED;
                    LOG_INFO("[RelayTransport] Registered as {}", did_);
                    // Start listening for INCOMING connections
                    start_incoming_listen();
                    callback(std::error_code());
                } else {
                    state_ = State::FAILED;
                    LOG_ERROR("[RelayTransport] Register failed: {}", line);
                    callback(std::make_error_code(std::errc::permission_denied));
                }
            });
        });
}

void RelayTransport::read_response(
    std::function<void(const std::string& line, const std::error_code&)> callback) {
    auto self = shared_from_this();
    auto timeout_timer = std::make_shared<boost::asio::steady_timer>(io_context_);
    auto completed = std::make_shared<std::atomic<bool>>(false);

    // 5 second timeout for reading response
    timeout_timer->expires_after(std::chrono::seconds(5));
    timeout_timer->async_wait([this, self, completed, callback](const boost::system::error_code& ec) {
        if (!ec && !completed->exchange(true)) {
            boost::system::error_code close_ec;
            socket_.close(close_ec);
            LOG_ERROR("[RelayTransport] Read response timed out");
            callback("", std::make_error_code(std::errc::timed_out));
        }
    });

    asio::async_read_until(socket_, response_buffer_, '\n',
        [this, self, callback, timeout_timer, completed](const boost::system::error_code& ec, std::size_t) {
            if (!completed->exchange(true)) {
                timeout_timer->cancel();
                if (ec) {
                    callback("", std::error_code(ec.value(), std::system_category()));
                    return;
                }

                std::istream is(&response_buffer_);
                std::string line;
                std::getline(is, line);
                // Remove \r if present
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }

                callback(line, std::error_code());
            }
        });
}

void RelayTransport::start_incoming_listen() {
    if (state_ != State::REGISTERED) {
        return;
    }

    auto self = shared_from_this();

    asio::async_read_until(socket_, response_buffer_, '\n',
        [this, self](const boost::system::error_code& ec, std::size_t) {
            if (ec) {
                if (state_ == State::REGISTERED) {
                    handle_disconnect(ec);
                }
                return;
            }

            if (state_ != State::REGISTERED) {
                // State changed (e.g., we initiated a CONNECT), ignore
                return;
            }

            std::istream is(&response_buffer_);
            std::string line;
            std::getline(is, line);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (line.substr(0, 9) == "INCOMING ") {
                std::string peer_did = line.substr(9);
                LOG_INFO("[RelayTransport] Incoming connection from {}", peer_did);

                state_ = State::RELAYING;

                if (event_callback_) {
                    event_callback_("connected", "incoming from " + peer_did);
                }

                // Start reading relay data
                start_relay_read();
            } else {
                LOG_WARN("[RelayTransport] Unexpected message while registered: {}", line);
                // Continue listening
                start_incoming_listen();
            }
        });
}

void RelayTransport::start_relay_read() {
    if (state_ != State::RELAYING) {
        return;
    }

    // Clear framer state from any prior relay session
    framer_.clear();

    // Flush any leftover data from response_buffer_ (from async_read_until)
    // before switching to raw socket reads — feed it through the framer
    if (response_buffer_.size() > 0) {
        auto leftover_size = response_buffer_.size();
        std::vector<uint8_t> leftover(leftover_size);
        std::istream is(&response_buffer_);
        is.read(reinterpret_cast<char*>(leftover.data()), leftover_size);

        auto messages = framer_.feed(leftover.data(), leftover.size());
        for (auto& msg : messages) {
            if (receive_callback_) {
                receive_callback_(msg);
            }
        }
    }

    do_relay_read();
}

void RelayTransport::do_relay_read() {
    if (state_ != State::RELAYING) {
        return;
    }

    auto self = shared_from_this();
    auto buf = std::make_shared<std::vector<uint8_t>>(8192);

    socket_.async_read_some(asio::buffer(*buf),
        [this, self, buf](const boost::system::error_code& ec, std::size_t bytes) {
            if (ec) {
                handle_disconnect(ec);
                return;
            }

            // Feed raw TCP bytes through the framer to extract complete messages
            auto messages = framer_.feed(buf->data(), bytes);
            for (auto& msg : messages) {
                if (receive_callback_) {
                    receive_callback_(msg);
                }
            }

            // Continue reading
            do_relay_read();
        });
}

void RelayTransport::do_write() {
    auto self = shared_from_this();
    std::shared_ptr<std::vector<uint8_t>> buf;
    SendCallback callback;

    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        if (write_queue_.empty()) return;
        buf = write_queue_.front().first;
        callback = write_queue_.front().second;
    }

    asio::async_write(socket_, asio::buffer(*buf),
        [this, self, buf, callback](const boost::system::error_code& ec, std::size_t) {
            if (ec) {
                handle_disconnect(ec);
                if (callback) {
                    callback(std::error_code(ec.value(), std::system_category()));
                }
                return;
            }

            if (callback) {
                callback(std::error_code());
            }

            std::lock_guard<std::mutex> lock(write_mutex_);
            write_queue_.pop_front();
            if (!write_queue_.empty()) {
                asio::post(socket_.get_executor(), [this, self]() {
                    do_write();
                });
            }
        });
}

void RelayTransport::handle_disconnect(const boost::system::error_code& ec) {
    if (ec != asio::error::eof && ec != asio::error::operation_aborted) {
        LOG_ERROR("[RelayTransport] Error: {}", ec.message());
    }

    auto prev_state = state_.load();
    state_ = State::DISCONNECTED;

    if (prev_state == State::RELAYING && event_callback_) {
        event_callback_("disconnected", ec.message());
    }

    boost::system::error_code close_ec;
    if (socket_.is_open()) {
        socket_.close(close_ec);
    }
}

} // namespace transport
} // namespace p2p
