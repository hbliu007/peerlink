/**
 * @file basic_client.cpp
 * @brief Basic P2P client usage example with relay support
 *
 * Usage: basic_client <my_did> <peer_did> [server_host] [relay_port] [max_retries]
 */

#include "p2p/core/p2p_client.hpp"
#include <iostream>
#include <string>
#include <memory>
#include <atomic>

using namespace p2p;

static std::atomic<bool> g_retrying{false};
static std::atomic<bool> g_connected{false};

// Forward declaration for retry connect
void retry_connect(std::shared_ptr<core::P2PClient> client,
                   boost::asio::io_context& io_context,
                   const std::string& peer_did,
                   const std::string& my_did,
                   int retries_left);

void retry_connect(std::shared_ptr<core::P2PClient> client,
                   boost::asio::io_context& io_context,
                   const std::string& peer_did,
                   const std::string& my_did,
                   int retries_left) {
    // Check if already connected (e.g., via INCOMING from peer)
    if (g_connected) {
        std::cout << "Already connected (incoming connection), stopping retries." << std::endl;
        return;
    }

    if (retries_left <= 0) {
        std::cerr << "Max retries reached. Peer " << peer_did << " not found." << std::endl;
        io_context.stop();
        return;
    }

    std::cout << "Connecting to peer: " << peer_did
              << " (" << retries_left << " retries left)" << std::endl;

    client->connect(peer_did, [&, client, retries_left](const std::error_code& ec) {
        if (!ec || g_connected) {
            if (!ec) std::cout << "Connection established!" << std::endl;
            return;
        }

        std::cerr << "Connect failed: " << ec.message()
                  << " — retrying in 3 seconds..." << std::endl;

        // Reset P2PClient state without disconnecting relay transport
        client->reset_for_retry();

        auto timer = std::make_shared<boost::asio::steady_timer>(io_context);
        timer->expires_after(std::chrono::seconds(3));
        timer->async_wait([&, client, timer, retries_left](const boost::system::error_code& ec) {
            if (ec || g_connected) return;
            retry_connect(client, io_context, peer_did, my_did, retries_left - 1);
        });
    });
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <my_did> <peer_did> [server_host] [relay_port] [max_retries]" << std::endl;
        return 1;
    }

    std::string my_did = argv[1];
    std::string peer_did = argv[2];
    std::string server_host = (argc > 3) ? argv[3] : "127.0.0.1";
    uint16_t relay_port = (argc > 4) ? static_cast<uint16_t>(std::stoi(argv[4])) : 9700;
    int max_retries = (argc > 5) ? std::stoi(argv[5]) : 60;

    std::cout << "PeerLink Client starting..." << std::endl;
    std::cout << "  My DID: " << my_did << std::endl;
    std::cout << "  Peer DID: " << peer_did << std::endl;
    std::cout << "  Server: " << server_host << std::endl;
    std::cout << "  Relay Port: " << relay_port << std::endl;
    std::cout << "  Max Retries: " << max_retries << std::endl;

    try {
        boost::asio::io_context io_context;

        core::P2PConfig config;
        config.signaling_server = server_host;
        config.signaling_port = 8080;
        config.stun_server = server_host;
        config.stun_port = 3478;
        config.relay_server = server_host;
        config.relay_port = 9001;
        config.tcp_relay_server = server_host;
        config.tcp_relay_port = relay_port;
        config.auto_relay = true;
        config.relay_mode = core::RelayMode::RELAY_ONLY;

        auto client = std::make_shared<core::P2PClient>(io_context, my_did, config);

        client->on_connected([&, client, my_did]() {
            g_connected = true;
            std::cout << "\n=== CONNECTED ===" << std::endl;
            std::cout << "  Peer: " << peer_did << std::endl;
            std::cout << "  Type: " << (client->is_p2p() ? "P2P" : "Relay") << std::endl;

            int channel_id = client->create_channel();
            std::cout << "  Channel: " << channel_id << std::endl;

            // Send test message — capture message by value to avoid dangling reference
            auto message = std::make_shared<std::string>("Hello from " + my_did + "!");
            std::vector<uint8_t> data(message->begin(), message->end());

            client->send_data(channel_id, data, [message](const std::error_code& ec) {
                if (ec) {
                    std::cerr << "  Send failed: " << ec.message() << std::endl;
                } else {
                    std::cout << "  Sent: \"" << *message << "\"" << std::endl;
                }
            });
        });

        client->on_disconnected([&]() {
            if (g_retrying) {
                std::cout << "Disconnected (retry in progress, ignoring)" << std::endl;
                return;
            }
            std::cout << "Disconnected from peer" << std::endl;
            if (g_connected) {
                io_context.stop();
            }
        });

        client->on_data([](int channel_id, const std::vector<uint8_t>& data) {
            std::string message(data.begin(), data.end());
            std::cout << "\n>>> Received on channel " << channel_id << ": \"" << message << "\"" << std::endl;
        });

        client->on_error([](const std::error_code& ec, const std::string& message) {
            std::cerr << "Error: " << message << " (" << ec.message() << ")" << std::endl;
        });

        // Initialize and connect with retry
        std::cout << "Initializing..." << std::endl;
        client->initialize([&, client](const std::error_code& ec) {
            if (ec) {
                std::cerr << "Initialization failed: " << ec.message() << std::endl;
                io_context.stop();
                return;
            }
            std::cout << "Initialized. Relay registered." << std::endl;
            retry_connect(client, io_context, peer_did, my_did, max_retries);
        });

        io_context.run();
        std::cout << "Client stopped" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
