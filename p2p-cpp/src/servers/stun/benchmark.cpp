#include "p2p/protocol/stun.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <chrono>
#include <vector>
#include <atomic>

namespace asio = boost::asio;

using namespace p2p::protocol;
using boost::asio::ip::udp;

class StunBenchmark {
public:
    StunBenchmark(const std::string& server_host, uint16_t server_port, size_t num_requests)
        : server_host_(server_host),
          server_port_(server_port),
          num_requests_(num_requests),
          completed_(0),
          success_(0),
          failed_(0) {}

    void run() {
        boost::asio::io_context io_context;

        auto start = std::chrono::high_resolution_clock::now();

        // Create multiple concurrent requests
        std::vector<std::shared_ptr<udp::socket>> sockets;

        for (size_t i = 0; i < num_requests_; ++i) {
            auto socket = std::make_shared<udp::socket>(io_context, udp::endpoint(udp::v4(), 0));
            sockets.push_back(socket);

            send_request(socket, i);
        }

        // Run io_context
        io_context.run();

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        // Print results
        std::cout << "\n=== STUN Server Benchmark ===" << std::endl;
        std::cout << "Total requests: " << num_requests_ << std::endl;
        std::cout << "Successful: " << success_ << std::endl;
        std::cout << "Failed: " << failed_ << std::endl;
        std::cout << "Duration: " << duration.count() << " ms" << std::endl;
        std::cout << "Requests/sec: " << (num_requests_ * 1000.0 / duration.count()) << std::endl;
        std::cout << "Avg latency: " << (duration.count() / static_cast<double>(num_requests_)) << " ms" << std::endl;
    }

private:
    void send_request(std::shared_ptr<udp::socket> socket, size_t request_id) {
        // Create transaction ID
        TransactionId tid;
        for (size_t i = 0; i < 12; ++i) {
            tid[i] = static_cast<uint8_t>((request_id + i) & 0xFF);
        }

        // Create binding request
        StunMessage request(StunMessageType::BindingRequest, tid);
        auto request_data = std::make_shared<std::vector<uint8_t>>(request.serialize());

        // Server endpoint
        auto server_endpoint = std::make_shared<udp::endpoint>(
            boost::asio::ip::make_address(server_host_),
            server_port_
        );

        // Send request
        socket->async_send_to(
            boost::asio::buffer(*request_data),
            *server_endpoint,
            [this, socket, tid](const boost::system::error_code& error, size_t) {
                if (!error) {
                    receive_response(socket, tid);
                } else {
                    failed_++;
                    completed_++;
                }
            }
        );
    }

    void receive_response(std::shared_ptr<udp::socket> socket, TransactionId tid) {
        auto recv_buffer = std::make_shared<std::array<uint8_t, 512>>();
        auto sender_endpoint = std::make_shared<udp::endpoint>();

        socket->async_receive_from(
            boost::asio::buffer(*recv_buffer),
            *sender_endpoint,
            [this, recv_buffer, tid](const boost::system::error_code& error, size_t bytes_transferred) {
                if (!error) {
                    // Parse response
                    auto response = StunMessage::parse(recv_buffer->data(), bytes_transferred);

                    if (response && response->transaction_id() == tid) {
                        success_++;
                    } else {
                        failed_++;
                    }
                } else {
                    failed_++;
                }

                completed_++;
            }
        );
    }

private:
    std::string server_host_;
    uint16_t server_port_;
    size_t num_requests_;
    std::atomic<size_t> completed_;
    std::atomic<size_t> success_;
    std::atomic<size_t> failed_;
};

int main(int argc, char* argv[]) {
    std::string server_host = "127.0.0.1";
    uint16_t server_port = 3478;
    size_t num_requests = 1000;

    if (argc > 1) {
        server_host = argv[1];
    }
    if (argc > 2) {
        server_port = static_cast<uint16_t>(std::stoi(argv[2]));
    }
    if (argc > 3) {
        num_requests = std::stoull(argv[3]);
    }

    std::cout << "STUN Server Benchmark" << std::endl;
    std::cout << "Server: " << server_host << ":" << server_port << std::endl;
    std::cout << "Requests: " << num_requests << std::endl;
    std::cout << "\nStarting benchmark..." << std::endl;

    StunBenchmark benchmark(server_host, server_port, num_requests);
    benchmark.run();

    return 0;
}
