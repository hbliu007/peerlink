/**
 * @file relay_tunnel_single.cpp
 * @brief Single-session TCP tunnel over Relay Server
 *
 * Simplified version: one relay connection = one SSH session
 * Connection closes after session ends (user can restart)
 *
 * Two modes:
 *   Server mode (office): accepts ONE relay connection, forwards to local SSH
 *   Client mode (home):   accepts ONE TCP connection, bridges to relay
 *
 * Usage:
 *   # Office side (forward relay traffic to local SSH):
 *   ./relay-tunnel server --did office-213 --relay your-relay.com:443 --forward 127.0.0.1:22
 *
 *   # Home side (local port 9022 -> office SSH):
 *   ./relay-tunnel client --did home-mac --target office-213 --relay your-relay.com:443 --listen 9022
 *
 *   # Then: ssh -p 9022 user@127.0.0.1
 *
 * Build:
 *   macOS:  g++ -std=c++17 -O2 -I/opt/homebrew/include -DBOOST_ASIO_NO_DEPRECATED -o relay-tunnel relay_tunnel_single.cpp -lpthread
 *   Linux:  g++ -std=c++17 -O2 -o relay-tunnel relay_tunnel_single.cpp -lpthread -lboost_system
 */

#include "relay_tunnel_common.hpp"
#include <csignal>
#include <atomic>

static std::atomic<bool> g_running{true};

// ─── Server Mode ────────────────────────────────────────────────────
void run_server(asio::io_context& io, const std::string& did,
                const std::string& relay_host, uint16_t relay_port,
                const std::string& forward_host, uint16_t forward_port) {

    std::cout << "[Server] Creating relay connection with DID: " << did << std::endl;

    auto relay = std::make_shared<RelayConnection>(io, relay_host, relay_port);

    relay->connect_and_register(did, [relay, forward_host, forward_port, &io]
                                (bool ok) {
        if (!ok) {
            std::cerr << "[Server] Failed to register, exiting" << std::endl;
            io.stop();
            return;
        }

        std::cout << "[Server] Waiting for client connection..." << std::endl;

        relay->wait_for_connection([relay, forward_host, forward_port, &io]
                                   (bool ok, const std::string& peer) {
            if (!ok) {
                std::cerr << "[Server] No client connected, exiting" << std::endl;
                io.stop();
                return;
            }

            std::cout << "[Server] Client connected: " << peer << std::endl;
            std::cout << "[Server] Connecting to local " << forward_host << ":" << forward_port << std::endl;

            // Connect to local service (e.g. SSH)
            auto local_socket = std::make_shared<tcp::socket>(relay->get_io_context());
            auto resolver = std::make_shared<tcp::resolver>(relay->get_io_context());

            resolver->async_resolve(forward_host, std::to_string(forward_port),
                [relay, local_socket, resolver, &io]
                (const boost::system::error_code& ec, tcp::resolver::results_type endpoints) {
                    if (ec) {
                        std::cerr << "[Server] Resolve failed: " << ec.message() << std::endl;
                        io.stop();
                        return;
                    }

                    asio::async_connect(*local_socket, endpoints,
                        [relay, local_socket, &io]
                        (const boost::system::error_code& ec, const tcp::endpoint& ep) {
                            if (ec) {
                                std::cerr << "[Server] Failed to connect to local service: " << ec.message() << std::endl;
                                io.stop();
                                return;
                            }

                            std::cout << "[Server] Connected to local service at " << ep << std::endl;
                            std::cout << "[Server] Tunnel active! Relaying data..." << std::endl;

                            auto bridge = std::make_shared<TcpBridge>(std::move(*local_socket), relay);

                            // Relay -> local service
                            relay->set_data_callback([bridge](const std::vector<uint8_t>& data) {
                                bridge->send_to_tcp(data);
                            });

                            relay->set_event_callback([bridge, relay, &io](const std::string& event) {
                                std::cout << "[Server] Relay event: " << event << std::endl;
                                bridge->close();
                                relay->close();
                                std::cout << "[Server] Session ended, exiting" << std::endl;
                                io.stop();
                            });

                            // Start both directions
                            relay->start_relay_read();
                            bridge->start();
                        });
                });
        });
    });
}

// ─── Client Mode ────────────────────────────────────────────────────
void run_client(asio::io_context& io, const std::string& did,
                const std::string& target_did,
                const std::string& relay_host, uint16_t relay_port,
                const std::string& listen_host, uint16_t listen_port) {

    // Start local TCP listener
    auto acceptor = std::make_shared<tcp::acceptor>(
        io, tcp::endpoint(asio::ip::make_address(listen_host), listen_port));
    std::cout << "[Client] Listening on " << listen_host << ":" << listen_port << std::endl;
    std::cout << "[Client] Tunnel ready! Use: ssh -p " << listen_port << " user@" << listen_host << std::endl;

    // Accept ONE connection
    std::cout << "[Client] Starting async_accept..." << std::endl;
    acceptor->async_accept([&io, did, target_did, relay_host, relay_port, acceptor]
                           (const boost::system::error_code& ec, tcp::socket socket) {
        std::cout << "[Client] async_accept callback triggered" << std::endl;
        if (ec) {
            std::cerr << "[Client] Accept error: " << ec.message()
                      << " (code: " << ec.value() << ")" << std::endl;
            io.stop();
            return;
        }

        std::cout << "[Client] TCP connection accepted" << std::endl;
        std::cout << "[Client] Creating relay connection with DID: " << did << std::endl;

        // Create relay connection
        auto relay = std::make_shared<RelayConnection>(io, relay_host, relay_port);
        auto bridge_socket = std::make_shared<tcp::socket>(std::move(socket));

        relay->connect_and_register(did, [relay, target_did, bridge_socket, &io]
                                    (bool ok) {
            if (!ok) {
                std::cerr << "[Client] Failed to register, exiting" << std::endl;
                io.stop();
                return;
            }

            relay->connect_to_peer(target_did, [relay, bridge_socket, &io]
                                   (bool ok) {
                if (!ok) {
                    std::cerr << "[Client] Failed to connect to target, exiting" << std::endl;
                    io.stop();
                    return;
                }

                std::cout << "[Client] Tunnel established, relaying data..." << std::endl;

                auto bridge = std::make_shared<TcpBridge>(std::move(*bridge_socket), relay);

                // Relay -> local TCP
                relay->set_data_callback([bridge](const std::vector<uint8_t>& data) {
                    bridge->send_to_tcp(data);
                });

                relay->set_event_callback([bridge, relay, &io](const std::string& event) {
                    std::cout << "[Client] Relay event: " << event << std::endl;
                    bridge->close();
                    relay->close();
                    std::cout << "[Client] Session ended, exiting" << std::endl;
                    io.stop();
                });

                // Start both directions
                relay->start_relay_read();
                bridge->start();
            });
        });
    });
}

// ─── Main ───────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage:" << std::endl;
        std::cerr << "  " << argv[0] << " server --did <DID> --relay <HOST:PORT> --forward <HOST:PORT>" << std::endl;
        std::cerr << "  " << argv[0] << " client --did <DID> --target <TARGET_DID> --relay <HOST:PORT> --listen <HOST:PORT>" << std::endl;
        std::cerr << std::endl;
        std::cerr << "Examples:" << std::endl;
        std::cerr << "  Office: " << argv[0] << " server --did office-213 --relay your-relay.com:443 --forward 127.0.0.1:22" << std::endl;
        std::cerr << "  Home:   " << argv[0] << " client --did home-mac --target office-213 --relay your-relay.com:443 --listen 127.0.0.1:9022" << std::endl;
        std::cerr << "  Phone:  " << argv[0] << " client --did home-mac --target office-213 --relay your-relay.com:443 --listen 0.0.0.0:9022" << std::endl;
        return 1;
    }

    std::string mode = argv[1];
    std::string did, target_did, relay_host, forward_host = "127.0.0.1";
    std::string listen_host = "127.0.0.1";
    uint16_t relay_port = 443, forward_port = 22, listen_port = 9022;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--did" && i + 1 < argc) {
            did = argv[++i];
        } else if (arg == "--target" && i + 1 < argc) {
            target_did = argv[++i];
        } else if (arg == "--relay" && i + 1 < argc) {
            std::string val = argv[++i];
            auto pos = val.find_last_of(':');
            if (pos != std::string::npos) {
                relay_host = val.substr(0, pos);
                relay_port = static_cast<uint16_t>(std::stoi(val.substr(pos + 1)));
            } else {
                relay_host = val;
            }
        } else if (arg == "--forward" && i + 1 < argc) {
            std::string val = argv[++i];
            auto pos = val.find_last_of(':');
            if (pos != std::string::npos) {
                forward_host = val.substr(0, pos);
                forward_port = static_cast<uint16_t>(std::stoi(val.substr(pos + 1)));
            }
        } else if (arg == "--listen" && i + 1 < argc) {
            std::string val = argv[++i];
            auto pos = val.find_last_of(':');
            if (pos != std::string::npos) {
                listen_host = val.substr(0, pos);
                listen_port = static_cast<uint16_t>(std::stoi(val.substr(pos + 1)));
            } else {
                listen_port = static_cast<uint16_t>(std::stoi(val));
            }
        }
    }

    if (did.empty() || relay_host.empty()) {
        std::cerr << "Error: --did and --relay are required" << std::endl;
        return 1;
    }

    // Ignore SIGPIPE (broken pipe when remote closes)
    std::signal(SIGPIPE, SIG_IGN);

    try {
        asio::io_context io;

        if (mode == "server") {
            std::cout << "=== Relay Tunnel Server (Single Session) ===" << std::endl;
            std::cout << "DID: " << did << std::endl;
            std::cout << "Relay: " << relay_host << ":" << relay_port << std::endl;
            std::cout << "Forward: " << forward_host << ":" << forward_port << std::endl;
            run_server(io, did, relay_host, relay_port, forward_host, forward_port);
        } else if (mode == "client") {
            if (target_did.empty()) {
                std::cerr << "Error: --target is required for client mode" << std::endl;
                return 1;
            }
            std::cout << "=== Relay Tunnel Client (Single Session) ===" << std::endl;
            std::cout << "DID: " << did << std::endl;
            std::cout << "Target: " << target_did << std::endl;
            std::cout << "Relay: " << relay_host << ":" << relay_port << std::endl;
            std::cout << "Listen: " << listen_host << ":" << listen_port << std::endl;
            run_client(io, did, target_did, relay_host, relay_port, listen_host, listen_port);
        } else {
            std::cerr << "Unknown mode: " << mode << " (use 'server' or 'client')" << std::endl;
            return 1;
        }

        io.run();

    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Tunnel closed." << std::endl;
    return 0;
}

