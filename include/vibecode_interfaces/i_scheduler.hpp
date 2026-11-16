// Abstract contract for time-ordered delivery queues. Define operations after the project rules are finalized.
#pragma once

namespace alert_service {
class IScheduler {
public:
    virtual ~IScheduler() = default;
};
} // namespace alert_service
