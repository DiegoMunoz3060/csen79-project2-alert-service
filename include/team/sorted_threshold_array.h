#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "kit/counters.h"

namespace team {

// Alternative Structure 2 (--impl=alt): Sorted Dynamic Array with binary search.
// Fast, cache-friendly O(log m + k + a) range queries, but O(m) element shifts
// on inserting a new threshold or removing an emptied threshold.
template <class Key = std::int64_t, class Value = std::uint64_t>
class SortedThresholdArray {
public:
    struct Entry {
        Key threshold;
        std::vector<Value> subscribers;

        Entry(const Key& k, const Value& v) : threshold(k), subscribers{v} {}
    };

    bool insert(const Key& threshold, const Value& sub_id) {
        std::size_t idx = lower_bound_index(threshold);
        if (idx < entries_.size()) {
            KIT_COUNT(stats_, comparisons);
            if (!(threshold < entries_[idx].threshold) && !(entries_[idx].threshold < threshold)) {
                KIT_COUNT(stats_, visits);
                entries_[idx].subscribers.push_back(sub_id);
                ++total_subscribers_;
                return true;
            }
        }
        if (entries_.size() == entries_.capacity()) {
            KIT_COUNT(stats_, allocations);
        }
        std::size_t shifted = entries_.size() - idx;
        if (shifted > 0) {
            KIT_COUNT_N(stats_, moves, shifted);
        }
        entries_.emplace(entries_.begin() + static_cast<std::ptrdiff_t>(idx), threshold, sub_id);
        ++total_subscribers_;
        return true;
    }

    bool erase(const Key& threshold, const Value& sub_id) {
        std::size_t idx = lower_bound_index(threshold);
        if (idx >= entries_.size()) {
            return false;
        }
        KIT_COUNT(stats_, comparisons);
        if (threshold < entries_[idx].threshold || entries_[idx].threshold < threshold) {
            return false;
        }
        auto& bucket = entries_[idx].subscribers;
        for (std::size_t i = 0; i < bucket.size(); ++i) {
            KIT_COUNT(stats_, visits);
            if (bucket[i] == sub_id) {
                if (i + 1 < bucket.size()) {
                    KIT_COUNT_N(stats_, moves, bucket.size() - i - 1);
                }
                bucket.erase(bucket.begin() + static_cast<std::ptrdiff_t>(i));
                --total_subscribers_;
                if (bucket.empty()) {
                    std::size_t shifted = entries_.size() - idx - 1;
                    if (shifted > 0) {
                        KIT_COUNT_N(stats_, moves, shifted);
                    }
                    entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(idx));
                }
                return true;
            }
        }
        return false;
    }

    // Visits every (threshold, sub_id) with new_price < threshold <= old_price,
    // ordered by threshold descending (highest first), then subscription insertion order.
    template <class Visitor>
    void visit_crossed(const Key& new_price, const Key& old_price, Visitor&& visitor) const {
        if (entries_.empty() || !(new_price < old_price)) {
            return;
        }
        std::size_t idx = upper_bound_index(old_price);
        while (idx > 0) {
            --idx;
            KIT_COUNT(stats_, visits);
            KIT_COUNT(stats_, comparisons);
            if (!(new_price < entries_[idx].threshold)) {
                break;
            }
            for (const Value& sub_id : entries_[idx].subscribers) {
                KIT_COUNT_EXTRA(stats_, "returned");
                visitor(entries_[idx].threshold, sub_id);
            }
        }
    }

    bool empty() const { return entries_.empty(); }
    std::size_t size() const { return entries_.size(); }
    std::size_t subscriber_count() const { return total_subscribers_; }

    bool check_invariants() const {
        std::size_t counted = 0;
        for (std::size_t i = 0; i < entries_.size(); ++i) {
            if (entries_[i].subscribers.empty()) {
                return false;
            }
            if (i > 0 && !(entries_[i - 1].threshold < entries_[i].threshold)) {
                return false;
            }
            counted += entries_[i].subscribers.size();
        }
        return counted == total_subscribers_;
    }

    const kit::Counters& stats() const { return stats_; }
    void reset_stats() { stats_.reset(); }

    static constexpr std::size_t node_bytes() { return sizeof(Entry); }

private:
    std::size_t lower_bound_index(const Key& key) const {
        std::size_t lo = 0;
        std::size_t hi = entries_.size();
        while (lo < hi) {
            std::size_t mid = lo + (hi - lo) / 2;
            KIT_COUNT(stats_, visits);
            KIT_COUNT(stats_, comparisons);
            if (entries_[mid].threshold < key) {
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }
        return lo;
    }

    std::size_t upper_bound_index(const Key& key) const {
        std::size_t lo = 0;
        std::size_t hi = entries_.size();
        while (lo < hi) {
            std::size_t mid = lo + (hi - lo) / 2;
            KIT_COUNT(stats_, visits);
            KIT_COUNT(stats_, comparisons);
            if (!(key < entries_[mid].threshold)) {
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }
        return lo;
    }

    std::vector<Entry> entries_;
    std::size_t total_subscribers_ = 0;
    mutable kit::Counters stats_;
};

}  // namespace team
