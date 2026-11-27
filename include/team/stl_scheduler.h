#pragma once

#include <cstddef>
#include <cstdint>
#include <queue>
#include <vector>

#include "kit/counters.h"

namespace team {

struct ScheduledEntry {
    std::int64_t due_minute = 0;
    std::uint64_t sub_id = 0;

    std::uint64_t sequence = 0;

    // Equal deadlines are ordered by the time they were scheduled.
    bool operator>(const ScheduledEntry& other) const {
        if (due_minute != other.due_minute) {
            return due_minute > other.due_minute;
        }
        return sequence > other.sequence;
    }
};

// STL baseline for Scheduled Delivery Min-Heap.
class StlDeliveryScheduler {
public:
    void push(const ScheduledEntry& entry) {
        KIT_COUNT(stats_, moves);
        KIT_COUNT_EXTRA(stats_, "pushes");
        ScheduledEntry scheduled = entry;
        scheduled.sequence = next_sequence_++;
        heap_.push(scheduled);
    }

    const ScheduledEntry& peek() const {
        KIT_COUNT(stats_, visits);
        return heap_.top();
    }

    ScheduledEntry pop() {
        KIT_COUNT(stats_, moves);
        KIT_COUNT_EXTRA(stats_, "pops");
        ScheduledEntry top = heap_.top();
        heap_.pop();
        return top;
    }

    bool empty() const { return heap_.empty(); }
    std::size_t size() const { return heap_.size(); }

    const kit::Counters& stats() const { return stats_; }
    void reset_stats() { stats_.reset(); }

private:
    std::priority_queue<ScheduledEntry, std::vector<ScheduledEntry>, std::greater<ScheduledEntry>> heap_;
    std::uint64_t next_sequence_ = 0;
    mutable kit::Counters stats_;
};

}  // namespace team
