// Abstract contract for hierarchical topic matching. Define operations after the project rules are finalized.
#pragma once

namespace alert_service {
class ITopicStore {
public:
    virtual ~ITopicStore() = default;
};
} // namespace alert_service
