// TODO: Define topic/user identifiers, timestamps, and event types.
#pragma once

#include <cstdint>

namespace alert_service {
using TopicId = std::uint64_t;
using UserId = std::uint64_t;
using Timestamp = std::uint64_t;

struct Event {
    // Add event fields when the command and output specifications are available.
};
} // namespace alert_service
