#include "p2p/protocol/relay_message.hpp"
#include "relay_v2.pb.h"
#include <stdexcept>

namespace p2p {
namespace protocol {

RelayMessageWrapper::RelayMessageWrapper()
    : message_(std::make_unique<relay::v2::CircuitRelay>()) {
}

RelayMessageWrapper::~RelayMessageWrapper() = default;

RelayMessageWrapper::RelayMessageWrapper(RelayMessageWrapper&&) noexcept = default;
RelayMessageWrapper& RelayMessageWrapper::operator=(RelayMessageWrapper&&) noexcept = default;

RelayMessageWrapper::RelayMessageWrapper(std::unique_ptr<relay::v2::CircuitRelay> msg)
    : message_(std::move(msg)) {
}

RelayMessageWrapper RelayMessageWrapper::CreateReserve() {
    auto msg = std::make_unique<relay::v2::CircuitRelay>();
    msg->set_type(relay::v2::CircuitRelay::RESERVE);
    return RelayMessageWrapper(std::move(msg));
}

RelayMessageWrapper RelayMessageWrapper::CreateConnect(const PeerInfo& peer) {
    auto msg = std::make_unique<relay::v2::CircuitRelay>();
    msg->set_type(relay::v2::CircuitRelay::CONNECT);

    auto* peer_msg = msg->mutable_peer();
    peer_msg->set_id(peer.id.data(), peer.id.size());
    for (const auto& addr : peer.addrs) {
        peer_msg->add_addrs(addr.data(), addr.size());
    }

    return RelayMessageWrapper(std::move(msg));
}

RelayMessageWrapper RelayMessageWrapper::CreateStatus(
    RelayStatusCode code,
    const std::string& text,
    const std::optional<ReservationInfo>& reservation) {

    auto msg = std::make_unique<relay::v2::CircuitRelay>();
    msg->set_type(relay::v2::CircuitRelay::STATUS);

    auto* status = msg->mutable_status();
    status->set_code(static_cast<relay::v2::Status::Code>(code));
    if (!text.empty()) {
        status->set_text(text);
    }

    if (reservation.has_value()) {
        auto* res = msg->mutable_reservation();
        res->set_expire(reservation->expire);
        res->set_addr(reservation->addr.data(), reservation->addr.size());
        res->set_voucher(reservation->voucher.data(), reservation->voucher.size());
        res->set_limit_duration(reservation->limit_duration);
        res->set_limit_data(reservation->limit_data);
    }

    return RelayMessageWrapper(std::move(msg));
}

std::vector<uint8_t> RelayMessageWrapper::Serialize() const {
    std::string serialized;
    if (!message_->SerializeToString(&serialized)) {
        throw std::runtime_error("Failed to serialize Relay message");
    }

    return std::vector<uint8_t>(serialized.begin(), serialized.end());
}

std::optional<RelayMessageWrapper> RelayMessageWrapper::Deserialize(
    const std::vector<uint8_t>& data) {

    auto msg = std::make_unique<relay::v2::CircuitRelay>();
    if (!msg->ParseFromArray(data.data(), data.size())) {
        return std::nullopt;
    }

    return RelayMessageWrapper(std::move(msg));
}

RelayMessageType RelayMessageWrapper::GetType() const {
    switch (message_->type()) {
        case relay::v2::CircuitRelay::RESERVE:
            return RelayMessageType::RESERVE;
        case relay::v2::CircuitRelay::CONNECT:
            return RelayMessageType::CONNECT;
        case relay::v2::CircuitRelay::STATUS:
            return RelayMessageType::STATUS;
        default:
            throw std::runtime_error("Unknown Relay message type");
    }
}

std::optional<PeerInfo> RelayMessageWrapper::GetPeer() const {
    if (message_->type() != relay::v2::CircuitRelay::CONNECT) {
        return std::nullopt;
    }

    if (!message_->has_peer()) {
        return std::nullopt;
    }

    PeerInfo peer;
    const auto& peer_msg = message_->peer();

    peer.id = std::vector<uint8_t>(peer_msg.id().begin(), peer_msg.id().end());

    for (int i = 0; i < peer_msg.addrs_size(); ++i) {
        const auto& addr_str = peer_msg.addrs(i);
        peer.addrs.emplace_back(addr_str.begin(), addr_str.end());
    }

    return peer;
}

RelayStatusCode RelayMessageWrapper::GetStatusCode() const {
    if (message_->type() != relay::v2::CircuitRelay::STATUS) {
        throw std::runtime_error("Not a STATUS message");
    }
    return static_cast<RelayStatusCode>(message_->status().code());
}

std::string RelayMessageWrapper::GetStatusText() const {
    if (message_->type() != relay::v2::CircuitRelay::STATUS) {
        throw std::runtime_error("Not a STATUS message");
    }
    return message_->status().text();
}

std::optional<ReservationInfo> RelayMessageWrapper::GetReservation() const {
    if (message_->type() != relay::v2::CircuitRelay::STATUS) {
        return std::nullopt;
    }

    if (!message_->has_reservation()) {
        return std::nullopt;
    }

    ReservationInfo res;
    const auto& res_msg = message_->reservation();

    res.expire = res_msg.expire();
    res.addr = std::vector<uint8_t>(res_msg.addr().begin(), res_msg.addr().end());
    res.voucher = std::vector<uint8_t>(res_msg.voucher().begin(), res_msg.voucher().end());
    res.limit_duration = res_msg.limit_duration();
    res.limit_data = res_msg.limit_data();

    return res;
}

}  // namespace protocol
}  // namespace p2p
