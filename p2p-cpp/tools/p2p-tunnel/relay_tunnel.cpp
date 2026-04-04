/**
 * @file relay_tunnel.cpp
 * @brief Lightweight TCP tunnel over Relay Server with multi-session support
 *
 * Two modes:
 *   Server mode (office): accepts multiple relay connections, each forwarded
 *                         to local TCP port (e.g. SSH 22)
 *   Client mode (home):   listens on local port, creates new relay connection
 *                         for each incoming TCP connection
 *
 * Multi-session architecture:
 *   - Each SSH connection gets its own relay session with unique DID
 *   - Client: home-mac-session-1, home-mac-session-2, ...
 *   - Server: office-213-session-1, office-213-session-2, ...
 *   - Multiple concurrent SSH sessions supported
 *   - Sessions are independent and cleaned up on disconnect
 *
 * Usage:
 *   # Office side (forward relay traffic to local SSH):
 *   ./relay-tunnel server --did office-213 --relay your-relay.com:9443 --forward 127.0.0.1:22
 *
 *   # Home side (local port 9022 -> office SSH):
 *   ./relay-tunnel client --did home-mac --target office-213 --relay your-relay.com:9443 --listen 9022
 *
 *   # Then: ssh -p 9022 user@127.0.0.1 (multiple sessions work!)
 *
 * Build:
 *   g++ -std=c++17 -O2 -o relay-tunnel relay_tunnel.cpp -lpthread -lboost_system
 */

#include "relay_tunnel_common.hpp"
#include <csignal>
#include <atomic>

static std::atomic<bool> g_running{true};

// ─── Server Mode ────────────────────────────────────────────────────
// Office side: Register with fixed DID, accept multiple client connections
//
// Architecture:
//   1. For each session: create new relay, register with base_did (e.g., "office-213")
//   2. Wait for CONNECT request from client
//   3. When connected, create local TCP connection and relay data
//   4. After session ends, repeat from step 1 for next session
//
// This ensures clients can always find the server using the same target DID.
void run_server(asio::io_context& io, const std::string& base_did,
                const std::string& relay_host, uint16_t relay_port,
                const std::string& forward_host, uint16_t forward_port) {

    // Session counter for tracking
    auto session_counter = std::make_shared<std::atomic<int>>(0);

    // Recursive function to create a new relay session
    std::function<void()> create_session;
    create_session = [&io, base_did, relay_host, relay_port, forward_host, forward_port,
                      session_counter, &create_session]() {

        int session_id = ++(*session_counter);
        std::cout << "\n[Server] Session #" << session_id << " - Creating relay connection with DID: "
                  << base_did << std::endl;

        auto relay = std::make_shared<RelayConnection>(io, relay_host, relay_port);

        relay->connect_and_register(base_did, [relay, forward_host, forward_port, session_id, &create_session]
                                    (bool ok) {
            if (!ok) {
                std::cerr << "[Server] Session #" << session_id << " - Failed to register" << std::endl;
                // Retry after a delay
                asio::steady_timer timer(relay->get_io_context());
                timer.expires_after(std::chrono::seconds(1));
                timer.wait();
                create_session();
                return;
            }

            std::cout << "[Server] Session #" << session_id << " - Registered, waiting for client connection..." << std::endl;

            relay->wait_for_connection([relay, forward_host, forward_port, session_id, &create_session]
                                       (bool ok, const std::string& peer) {
                if (!ok) {
                    std::cerr << "[Server] Session #" << session_id << " - No client connected" << std::endl;
                    // Retry
                    create_session();
                    return;
                }

                std::cout << "[Server] Session #" << session_id << " - Client connected: " << peer << std::endl;
                std::cout << "[Server] Session #" << session_id << " - Connecting to local "
                          << forward_host << ":" << forward_port << std::endl;

                // Connect to local service (e.g. SSH)
                auto local_socket = std::make_shared<tcp::socket>(relay->get_io_context());
                auto resolver = std::make_shared<tcp::resolver>(relay->get_io_context());

                resolver->async_resolve(forward_host, std::to_string(forward_port),
                    [relay, local_socket, resolver, session_id, &create_session]
                    (const boost::system::error_code& ec, tcp::resolver::results_type endpoints) {
                        if (ec) {
                            std::cerr << "[Server] Session #" << session_id
                                      << " - Resolve failed: " << ec.message() << std::endl;
                            create_session();
                            return;
                        }

                        asio::async_connect(*local_socket, endpoints,
                            [relay, local_socket, session_id, &create_session]
                            (const boost::system::error_code& ec, const tcp::endpoint& ep) {
                                if (ec) {
                                    std::cerr << "[Server] Session #" << session_id
                                              << " - Failed to connect to local service: " << ec.message() << std::endl;
                                    create_session();
                                    return;
                                }

                                std::cout << "[Server] Session #" << session_id
                                          << " - Connected to local service at " << ep << std::endl;
                                std::cout << "[Server] Session #" << session_id
                                          << " - Tunnel active! Relaying data..." << std::endl;

                                auto bridge = std::make_shared<TcpBridge>(std::move(*local_socket), relay);

                                // Relay -> local service
                                relay->set_data_callback([bridge](const std::vector<uint8_t>& data) {
                                    bridge->send_to_tcp(data);
                                });

                                relay->set_event_callback([bridge, relay, session_id, &create_session](const std::string& event) {
                                    std::cout << "[Server] Session #" << session_id
                                              << " - Relay event: " << event << std::endl;
                                    bridge->close();
                                    relay->close();
                                    std::cout << "[Server] Session #" << session_id << " - Cleaned up" << std::endl;
                                    // Create next session
                                    create_session();
                                });

                                // Start both directions
                                relay->start_relay_read();
                                bridge->start();
                            });
                    });
            });
        });
    };

    // Start first session
    std::cout << "[Server] Starting multi-session server..." << std::endl;
    create_session();
}

// ─── Client Mode ────────────────────────────────────────────────────
// Home side: listen on local port, for each connection create new relay session
void run_client(asio::io_context& io, const std::string& base_did,
                const std::string& target_did,
                const std::string& relay_host, uint16_t relay_port,
                uint16_t listen_port) {

    // Start local TCP listener
    auto acceptor = std::make_shared<tcp::acceptor>(io, tcp::endpoint(tcp::v4(), listen_port));
    std::cout << "[Client] Listening on 127.0.0.1:" << listen_port << std::endl;
    std::cout << "[Client] Tunnel ready! Use: ssh -p " << listen_port << " user@127.0.0.1" << std::endl;

    // Session counter for unique DIDs
    auto session_counter = std::make_shared<std::atomic<int>>(0);

    // Recursive accept function
    std::function<void()> do_accept;
    do_accept = [&io, acceptor, base_did, target_did, relay_host, relay_port, session_counter, &do_accept]() {
        acceptor->async_accept([&io, acceptor, base_did, target_did, relay_host, relay_port, session_counter, &do_accept]
                               (const boost::system::error_code& ec, tcp::socket socket) {
            if (ec) {
                std::cerr << "[Client] Accept error: " << ec.message() << std::endl;
                // Continue accepting even on error
                do_accept();
                return;
            }

            int session_id = ++(*session_counter);
            std::string session_did = base_did + "-session-" + std::to_string(session_id);
            std::cout << "\n[Client] Session #" << session_id << " - TCP connection accepted" << std::endl;
            std::cout << "[Client] Creating new relay connection with DID: " << session_did << std::endl;

            // Create new relay connection for this session
            auto relay = std::make_shared<RelayConnection>(io, relay_host, relay_port);
            auto bridge_socket = std::make_shared<tcp::socket>(std::move(socket));

            relay->connect_and_register(session_did, [relay, target_did, bridge_socket, session_id]
                                        (bool ok) {
                if (!ok) {
                    std::cerr << "[Client] Session #" << session_id << " - Failed to register" << std::endl;
                    return;
                }

                relay->connect_to_peer(target_did, [relay, bridge_socket, session_id]
                                       (bool ok) {
                    if (!ok) {
                        std::cerr << "[Client] Session #" << session_id << " - Failed to connect to target" << std::endl;
                        return;
                    }

                    std::cout << "[Client] Session #" << session_id << " - Tunnel established, relaying data..." << std::endl;

                    auto bridge = std::make_shared<TcpBridge>(std::move(*bridge_socket), relay);

                    // Relay -> local TCP
                    relay->set_data_callback([bridge](const std::vector<uint8_t>& data) {
                        bridge->send_to_tcp(data);
                    });

                    relay->set_event_callback([bridge, relay, session_id](const std::string& event) {
                        std::cout << "[Client] Session #" << session_id << " - Relay event: " << event << std::endl;
                        bridge->close();
                        relay->close();
                        std::cout << "[Client] Session #" << session_id << " - Cleaned up" << std::endl;
                    });

                    // Start both directions
                    relay->start_relay_read();
                    bridge->start();
                });
            });

            // Continue accepting new connections
            do_accept();
        });
    };

    // Start accepting
    do_accept();
}

// ─── Main ───────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage:" << std::endl;
        std::cerr << "  " << argv[0] << " server --did <DID> --relay <HOST:PORT> --forward <HOST:PORT>" << std::endl;
        std::cerr << "  " << argv[0] << " client --did <DID> --target <TARGET_DID> --relay <HOST:PORT> --listen <PORT>" << std::endl;
        std::cerr << std::endl;
        std::cerr << "Examples:" << std::endl;
        std::cerr << "  Office: " << argv[0] << " server --did office-213 --relay your-relay.com:9443 --forward 127.0.0.1:22" << std::endl;
        std::cerr << "  Home:   " << argv[0] << " client --did home-mac --target office-213 --relay your-relay.com:9443 --listen 9022" << std::endl;
        return 1;
    }

    std::string mode = argv[1];
    std::string did, target_did, relay_host, forward_host = "127.0.0.1";
    uint16_t relay_port = 9443, forward_port = 22, listen_port = 9022;

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
            listen_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        }
    }

    if (did.empty() || relay_host.empty()) {
        std::cerr << "Error: --did and --relay are required" << std::endl;
        return 1;
    }

    std::signal(SIGINT, [](int) { g_running = false; });
    std::signal(SIGTERM, [](int) { g_running = false; });

    try {
        asio::io_context io;

        asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code&, int) {
            std::cout << "\nShutting down..." << std::endl;
            io.stop();
        });

        if (mode == "server") {
            std::cout << "=== Relay Tunnel Server ===" << std::endl;
            std::cout << "DID: " << did << std::endl;
            std::cout << "Relay: " << relay_host << ":" << relay_port << std::endl;
            std::cout << "Forward: " << forward_host << ":" << forward_port << std::endl;
            run_server(io, did, relay_host, relay_port, forward_host, forward_port);
        } else if (mode == "client") {
            if (target_did.empty()) {
                std::cerr << "Error: --target is required for client mode" << std::endl;
                return 1;
            }
            std::cout << "=== Relay Tunnel Client ===" << std::endl;
            std::cout << "DID: " << did << std::endl;
            std::cout << "Target: " << target_did << std::endl;
            std::cout << "Relay: " << relay_host << ":" << relay_port << std::endl;
            std::cout << "Listen: 127.0.0.1:" << listen_port << std::endl;
            run_client(io, did, target_did, relay_host, relay_port, listen_port);
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
