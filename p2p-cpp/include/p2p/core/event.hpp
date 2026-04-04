#pragma once

#include "types.hpp"
#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace p2p {

// Event types
enum class EventType {
    CONNECTION_OPENED,
    CONNECTION_CLOSED,
    CONNECTION_ERROR,
    DATA_RECEIVED,
    PEER_DISCOVERED,
    NAT_TYPE_DETECTED
};

// Event data
struct Event {
    EventType type;
    ConnectionId connection_id;
    std::string peer_id;
    std::vector<uint8_t> data;
    std::string error_message;
};

// Event callback
using EventCallback = std::function<void(const Event&)>;

// Event bus for pub/sub
class EventBus {
public:
    virtual ~EventBus() = default;

    // Subscribe to events
    virtual void Subscribe(EventType type, EventCallback callback) = 0;

    // Publish event
    virtual void Publish(const Event& event) = 0;

    // Unsubscribe all
    virtual void UnsubscribeAll() = 0;
};

// Create event bus
std::unique_ptr<EventBus> CreateEventBus();

} // namespace p2p