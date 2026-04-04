#include "p2p/nat/stun_client.hpp"
#include <random>
#include <cstring>
#include <atomic>
#include <arpa/inet.h>

namespace asio = boost::asio;

namespace p2p {
namespace nat {

STUNClient::STUNClient(boost::asio::io_context& io_context,
                       const std::string& stun_server,
                       uint16_t stun_port)
    : io_context_(io_context)
    , stun_server_(stun_server)
    , stun_port_(stun_port)
    , timeout_(5)
{
}

std::vector<uint8_t> STUNClient::packStunRequest() {
    std::vector<uint8_t> request;
    request.reserve(20);

    // STUN magic cookie
    const uint8_t magic_cookie[] = {0x21, 0x12, 0xA4, 0x42};

    // Transaction ID (12 bytes random)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    std::vector<uint8_t> transaction_id(12);
    for (auto& byte : transaction_id) {
        byte = static_cast<uint8_t>(dis(gen));
    }

    // Message type (BINDING_REQUEST)
    request.push_back((BINDING_REQUEST >> 8) & 0xFF);
    request.push_back(BINDING_REQUEST & 0xFF);

    // Message length (0 - no attributes)
    request.push_back(0);
    request.push_back(0);

    // Magic cookie
    request.insert(request.end(), magic_cookie, magic_cookie + 4);

    // Transaction ID
    request.insert(request.end(), transaction_id.begin(), transaction_id.end());

    return request;
}

std::pair<std::optional<std::string>, std::optional<uint16_t>>
STUNClient::unpackStunResponse(const std::vector<uint8_t>& data) {
    if (data.size() < 20) {
        return {std::nullopt, std::nullopt};
    }

    // Verify magic cookie
    const uint8_t expected_magic[] = {0x21, 0x12, 0xA4, 0x42};
    if (std::memcmp(&data[4], expected_magic, 4) != 0) {
        return {std::nullopt, std::nullopt};
    }

    // Parse message length
    uint16_t message_length = (static_cast<uint16_t>(data[2]) << 8) | data[3];

    // Parse attributes
    size_t idx = 20;
    while (idx + 4 <= data.size()) {
        uint16_t attr_type = (static_cast<uint16_t>(data[idx]) << 8) | data[idx + 1];
        uint16_t attr_length = (static_cast<uint16_t>(data[idx + 2]) << 8) | data[idx + 3];
        idx += 4;

        if (idx + attr_length > data.size()) {
            break;
        }

        // Parse XOR-MAPPED-ADDRESS (preferred)
        if (attr_type == ATTR_XOR_MAPPED_ADDRESS && attr_length >= 8) {
            uint8_t family = data[idx + 1];
            if (family == 0x01) {  // IPv4
                // XOR port with magic cookie high 16 bits
                uint16_t xored_port = (static_cast<uint16_t>(data[idx + 2]) << 8) | data[idx + 3];
                uint16_t port = xored_port ^ 0x2112;

                // XOR IP with magic cookie
                uint32_t xored_ip = (static_cast<uint32_t>(data[idx + 4]) << 24) |
                                   (static_cast<uint32_t>(data[idx + 5]) << 16) |
                                   (static_cast<uint32_t>(data[idx + 6]) << 8) |
                                   static_cast<uint32_t>(data[idx + 7]);
                uint32_t ip = xored_ip ^ 0x2112A442;

                // Convert to string
                char ip_str[INET_ADDRSTRLEN];
                struct in_addr addr;
                addr.s_addr = htonl(ip);
                inet_ntop(AF_INET, &addr, ip_str, INET_ADDRSTRLEN);

                return {std::string(ip_str), port};
            }
        }
        // Parse MAPPED-ADDRESS (fallback)
        else if (attr_type == ATTR_MAPPED_ADDRESS && attr_length >= 8) {
            uint8_t family = data[idx + 1];
            if (family == 0x01) {  // IPv4
                uint16_t port = (static_cast<uint16_t>(data[idx + 2]) << 8) | data[idx + 3];
                uint32_t ip = (static_cast<uint32_t>(data[idx + 4]) << 24) |
                             (static_cast<uint32_t>(data[idx + 5]) << 16) |
                             (static_cast<uint32_t>(data[idx + 6]) << 8) |
                             static_cast<uint32_t>(data[idx + 7]);

                char ip_str[INET_ADDRSTRLEN];
                struct in_addr addr;
                addr.s_addr = htonl(ip);
                inet_ntop(AF_INET, &addr, ip_str, INET_ADDRSTRLEN);

                return {std::string(ip_str), port};
            }
        }

        // Move to next attribute (with padding)
        size_t padding = (4 - (attr_length % 4)) % 4;
        idx += attr_length + padding;
    }

    return {std::nullopt, std::nullopt};
}

void STUNClient::send_request(std::function<void(
    std::optional<std::string> public_ip,
    std::optional<uint16_t> public_port
)> callback) {
    auto socket = std::make_shared<boost::asio::ip::udp::socket>(io_context_);
    auto timeout_timer = std::make_shared<boost::asio::steady_timer>(io_context_);
    auto completed = std::make_shared<std::atomic<bool>>(false);

    try {
        socket->open(boost::asio::ip::udp::v4());
        socket->bind(boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0));
    } catch (const std::exception&) {
        callback(std::nullopt, std::nullopt);
        return;
    }

    // Set timeout — cancel socket if no response within timeout_
    timeout_timer->expires_after(timeout_);
    timeout_timer->async_wait([socket, completed, callback](const boost::system::error_code& ec) {
        if (!ec && !completed->exchange(true)) {
            boost::system::error_code close_ec;
            socket->close(close_ec);
            callback(std::nullopt, std::nullopt);
        }
    });

    // Resolve server address
    auto resolver = std::make_shared<boost::asio::ip::udp::resolver>(io_context_);
    resolver->async_resolve(
        stun_server_,
        std::to_string(stun_port_),
        [this, socket, callback, resolver, timeout_timer, completed](
            const boost::system::error_code& ec,
            boost::asio::ip::udp::resolver::results_type results) {

            if (ec || results.empty()) {
                if (!completed->exchange(true)) {
                    timeout_timer->cancel();
                    callback(std::nullopt, std::nullopt);
                }
                return;
            }

            auto endpoint = *results.begin();
            auto request = packStunRequest();

            // Send request
            socket->async_send_to(
                boost::asio::buffer(request),
                endpoint,
                [this, socket, callback, timeout_timer, completed](const boost::system::error_code& ec, std::size_t) {
                    if (ec) {
                        if (!completed->exchange(true)) {
                            timeout_timer->cancel();
                            callback(std::nullopt, std::nullopt);
                        }
                        return;
                    }

                    // Receive response
                    auto recv_buffer = std::make_shared<std::vector<uint8_t>>(512);
                    auto sender_endpoint = std::make_shared<boost::asio::ip::udp::endpoint>();

                    socket->async_receive_from(
                        boost::asio::buffer(*recv_buffer),
                        *sender_endpoint,
                        [this, recv_buffer, callback, timeout_timer, completed](
                            const boost::system::error_code& ec,
                            std::size_t bytes_received) {

                            if (!completed->exchange(true)) {
                                timeout_timer->cancel();
                                if (ec) {
                                    callback(std::nullopt, std::nullopt);
                                    return;
                                }
                                recv_buffer->resize(bytes_received);
                                auto [ip, port] = unpackStunResponse(*recv_buffer);
                                callback(ip, port);
                            }
                        }
                    );
                }
            );
        }
    );
}

void detect_nat_type(
    boost::asio::io_context& io_context,
    const std::string& stun_server,
    uint16_t stun_port,
    std::function<void(const NATDetectionResult&)> callback
) {
    auto client = std::make_shared<STUNClient>(io_context, stun_server, stun_port);

    client->send_request([callback](
        std::optional<std::string> public_ip,
        std::optional<uint16_t> public_port) {

        NATDetectionResult result;

        if (!public_ip || !public_port) {
            result.nat_type = NATType::BLOCKED;
            callback(result);
            return;
        }

        result.public_ip = public_ip;
        result.public_port = public_port;

        // Simple detection: if we got a response, assume RESTRICTED_CONE
        // Full RFC 3489 detection would require multiple STUN servers
        result.nat_type = NATType::RESTRICTED_CONE;

        callback(result);
    });
}

bool is_nat_p2p_capable(NATType nat_type) {
    return nat_type == NATType::PUBLIC_IP ||
           nat_type == NATType::FULL_CONE ||
           nat_type == NATType::RESTRICTED_CONE ||
           nat_type == NATType::PORT_RESTRICTED_CONE;
}

} // namespace nat
} // namespace p2p
