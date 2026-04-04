#include "p2p/protocol/dcutr_message.hpp"
#include "dcutr.pb.h"
#include <stdexcept>

namespace p2p {
namespace protocol {

DCUtRMessageWrapper::DCUtRMessageWrapper()
    : message_(std::make_unique<dcutr::DCUtRMessage>()) {
}

DCUtRMessageWrapper::~DCUtRMessageWrapper() = default;

DCUtRMessageWrapper::DCUtRMessageWrapper(DCUtRMessageWrapper&&) noexcept = default;
DCUtRMessageWrapper& DCUtRMessageWrapper::operator=(DCUtRMessageWrapper&&) noexcept = default;

DCUtRMessageWrapper::DCUtRMessageWrapper(std::unique_ptr<dcutr::DCUtRMessage> msg)
    : message_(std::move(msg)) {
}

DCUtRMessageWrapper DCUtRMessageWrapper::CreateConnect(
    const std::vector<std::vector<uint8_t>>& addrs,
    int64_t timestamp_ns) {

    auto msg = std::make_unique<dcutr::DCUtRMessage>();
    msg->set_type(dcutr::DCUtRMessage::CONNECT);

    auto* connect = msg->mutable_connect();
    for (const auto& addr : addrs) {
        connect->add_addrs(addr.data(), addr.size());
    }
    connect->set_timestamp_ns(timestamp_ns);

    return DCUtRMessageWrapper(std::move(msg));
}

DCUtRMessageWrapper DCUtRMessageWrapper::CreateSync(
    const std::vector<std::vector<uint8_t>>& addrs,
    int64_t echo_timestamp_ns,
    int64_t timestamp_ns) {

    auto msg = std::make_unique<dcutr::DCUtRMessage>();
    msg->set_type(dcutr::DCUtRMessage::SYNC);

    auto* sync = msg->mutable_sync();
    for (const auto& addr : addrs) {
        sync->add_addrs(addr.data(), addr.size());
    }
    sync->set_echo_timestamp_ns(echo_timestamp_ns);
    sync->set_timestamp_ns(timestamp_ns);

    return DCUtRMessageWrapper(std::move(msg));
}

std::vector<uint8_t> DCUtRMessageWrapper::Serialize() const {
    std::string serialized;
    if (!message_->SerializeToString(&serialized)) {
        throw std::runtime_error("Failed to serialize DCUtR message");
    }

    return std::vector<uint8_t>(serialized.begin(), serialized.end());
}

std::optional<DCUtRMessageWrapper> DCUtRMessageWrapper::Deserialize(
    const std::vector<uint8_t>& data) {

    auto msg = std::make_unique<dcutr::DCUtRMessage>();
    if (!msg->ParseFromArray(data.data(), data.size())) {
        return std::nullopt;
    }

    return DCUtRMessageWrapper(std::move(msg));
}

DCUtRMessageWrapper::Type DCUtRMessageWrapper::GetType() const {
    switch (message_->type()) {
        case dcutr::DCUtRMessage::CONNECT:
            return Type::CONNECT;
        case dcutr::DCUtRMessage::SYNC:
            return Type::SYNC;
        default:
            throw std::runtime_error("Unknown DCUtR message type");
    }
}

std::vector<std::vector<uint8_t>> DCUtRMessageWrapper::GetConnectAddrs() const {
    if (message_->type() != dcutr::DCUtRMessage::CONNECT) {
        throw std::runtime_error("Not a CONNECT message");
    }

    std::vector<std::vector<uint8_t>> addrs;
    const auto& connect = message_->connect();
    for (int i = 0; i < connect.addrs_size(); ++i) {
        const auto& addr_str = connect.addrs(i);
        addrs.emplace_back(addr_str.begin(), addr_str.end());
    }
    return addrs;
}

int64_t DCUtRMessageWrapper::GetConnectTimestamp() const {
    if (message_->type() != dcutr::DCUtRMessage::CONNECT) {
        throw std::runtime_error("Not a CONNECT message");
    }
    return message_->connect().timestamp_ns();
}

std::vector<std::vector<uint8_t>> DCUtRMessageWrapper::GetSyncAddrs() const {
    if (message_->type() != dcutr::DCUtRMessage::SYNC) {
        throw std::runtime_error("Not a SYNC message");
    }

    std::vector<std::vector<uint8_t>> addrs;
    const auto& sync = message_->sync();
    for (int i = 0; i < sync.addrs_size(); ++i) {
        const auto& addr_str = sync.addrs(i);
        addrs.emplace_back(addr_str.begin(), addr_str.end());
    }
    return addrs;
}

int64_t DCUtRMessageWrapper::GetSyncEchoTimestamp() const {
    if (message_->type() != dcutr::DCUtRMessage::SYNC) {
        throw std::runtime_error("Not a SYNC message");
    }
    return message_->sync().echo_timestamp_ns();
}

int64_t DCUtRMessageWrapper::GetSyncTimestamp() const {
    if (message_->type() != dcutr::DCUtRMessage::SYNC) {
        throw std::runtime_error("Not a SYNC message");
    }
    return message_->sync().timestamp_ns();
}

}  // namespace protocol
}  // namespace p2p
