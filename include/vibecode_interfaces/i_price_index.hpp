// Abstract contract for price-threshold lookups. Define operations after the project rules are finalized.
#pragma once

namespace alert_service {
class IPriceIndex {
public:
    virtual ~IPriceIndex() = default;
};
} // namespace alert_service
