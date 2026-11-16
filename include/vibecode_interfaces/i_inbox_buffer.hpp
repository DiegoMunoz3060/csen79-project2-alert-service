// Abstract contract for bounded user inboxes and alert history. Define operations after the project rules are finalized.
#pragma once

namespace alert_service {
class IInboxBuffer {
public:
    virtual ~IInboxBuffer() = default;
};
} // namespace alert_service
