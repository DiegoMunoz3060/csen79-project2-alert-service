#pragma once

#include <algorithm>
#include <cstddef>
#include <deque>
#include <utility>

#include "kit/counters.h"

namespace team {

// STL baseline for Core Structure 3 (Bounded K-capacity Inbox).
template <class T>
class StlBoundedInbox {
public:
    explicit StlBoundedInbox(std::size_t capacity = 10) : capacity_(capacity) {}

    void set_capacity(std::size_t capacity) {
        capacity_ = capacity;
        while (items_.size() > capacity_) {
            items_.pop_front();
            KIT_COUNT(stats_, moves);
        }
    }

    void push(const T& item) {
        if (capacity_ == 0) {
            return;
        }
        if (items_.size() == capacity_) {
            items_.pop_front();
            KIT_COUNT(stats_, moves);
        }
        items_.push_back(item);
        KIT_COUNT(stats_, moves);
    }

    void push(T&& item) {
        if (capacity_ == 0) {
            return;
        }
        if (items_.size() == capacity_) {
            items_.pop_front();
            KIT_COUNT(stats_, moves);
        }
        items_.push_back(std::move(item));
        KIT_COUNT(stats_, moves);
    }

    // Visits up to the last `limit` alerts in oldest-first order.
    template <class Visitor>
    void visit_last_oldest_first(std::size_t limit, Visitor&& visitor) const {
        if (limit == 0 || items_.empty()) {
            return;
        }
        std::size_t count = std::min(limit, items_.size());
        std::size_t start = items_.size() - count;
        for (std::size_t i = start; i < items_.size(); ++i) {
            KIT_COUNT(stats_, visits);
            visitor(items_[i]);
        }
    }

    bool empty() const { return items_.empty(); }
    std::size_t size() const { return items_.size(); }
    std::size_t capacity() const { return capacity_; }

    const kit::Counters& stats() const { return stats_; }
    void reset_stats() { stats_.reset(); }

private:
    std::size_t capacity_ = 10;
    std::deque<T> items_;
    mutable kit::Counters stats_;
};

}  // namespace team
