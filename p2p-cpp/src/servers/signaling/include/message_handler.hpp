#pragma once

#include "message_router.hpp"

namespace signaling {

// Backward compatibility wrapper
// MessageHandler is now implemented by MessageRouter
using MessageHandler = MessageRouter;

} // namespace signaling