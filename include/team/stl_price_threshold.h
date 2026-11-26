#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

#include "kit/counters.h"

namespace team {

// STL baseline for Core Structure 2 (Price Threshold Index).
// Exposes the exact same interface as PriceThresholdAVL and SortedThresholdArray.
template <class Key = std::int64_t, class Value = std::uint64_t>
class StlPriceThresholdMap {
public:
    bool insert(const Key& threshold, const Value& sub_id) {
        KIT_COUNT(stats_, visits);
        auto it = tree_.find(threshold);
        KIT_COUNT(stats_, comparisons);
        if (it == tree_.end()) {
            KIT_COUNT(stats_, allocations);
            auto [inserted_it, ok] = tree_.emplace(threshold, std::vector<Value>{sub_id});
            (void)inserted_it;
            (void)ok;
            ++total_subscribers_;
            return true;
        }
        it->second.push_back(sub_id);
        ++total_subscribers_;
        return true;
    }

    bool erase(const Key& threshold, const Value& sub_id) {
        KIT_COUNT(stats_, visits);
        auto it = tree_.find(threshold);
        KIT_COUNT(stats_, comparisons);
        if (it == tree_.end()) {
            return false;
        }
        auto& bucket = it->second;
        for (std::size_t i = 0; i < bucket.size(); ++i) {
            KIT_COUNT(stats_, visits);
            if (bucket[i] == sub_id) {
                if (i + 1 < bucket.size()) {
                    KIT_COUNT_N(stats_, moves, bucket.size() - i - 1);
                }
                bucket.erase(bucket.begin() + static_cast<std::ptrdiff_t>(i));
                --total_subscribers_;
                if (bucket.empty()) {
                    tree_.erase(it);
                }
                return true;
            }
        }
        return false;
    }

    // Visits every (threshold, sub_id) with new_price < threshold <= old_price,
    // ordered by threshold descending (highest first), then insertion order.
    template <class Visitor>
    void visit_crossed(const Key& new_price, const Key& old_price, Visitor&& visitor) const {
        if (tree_.empty() || !(new_price < old_price)) {
            return;
        }
        auto it = tree_.upper_bound(old_price);
        KIT_COUNT(stats_, comparisons);
        while (it != tree_.begin()) {
            --it;
            KIT_COUNT(stats_, visits);
            KIT_COUNT(stats_, comparisons);
            if (!(new_price < it->first)) {
                break;
            }
            for (const Value& sub_id : it->second) {
                KIT_COUNT_EXTRA(stats_, "returned");
                visitor(it->first, sub_id);
            }
        }
    }

    bool empty() const { return tree_.empty(); }
    std::size_t size() const { return tree_.size(); }
    std::size_t subscriber_count() const { return total_subscribers_; }

    bool check_invariants() const {
        std::size_t counted = 0;
        for (const auto& [k, bucket] : tree_) {
            (void)k;
            if (bucket.empty()) {
                return false;
            }
            counted += bucket.size();
        }
        return counted == total_subscribers_;
    }

    const kit::Counters& stats() const { return stats_; }
    void reset_stats() { stats_.reset(); }

    static constexpr std::size_t node_bytes() {
        // Approximate libstdc++/libc++ std::map node header (32B) + pair<Key, vector<Value>>
        return 32 + sizeof(std::pair<const Key, std::vector<Value>>);
    }

private:
    std::map<Key, std::vector<Value>> tree_;
    std::size_t total_subscribers_ = 0;
    mutable kit::Counters stats_;
};

}  // namespace team
