#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "kit/counters.h"

namespace team {

// STL baseline for Core Structure 2 (Price Threshold Index).
// Exposes the exact same interface as PriceThresholdAVL and SortedThresholdArray,
// using a counting comparator so internal std::map key comparisons are recorded fairly.
template <class Key = std::int64_t, class Value = std::uint64_t>
class StlPriceThresholdMap {
public:
    struct CountingLess {
        kit::Counters* stats = nullptr;

        bool operator()(const Key& a, const Key& b) const {
            if (stats) {
                KIT_COUNT(*stats, comparisons);
            }
            return a < b;
        }
    };

    StlPriceThresholdMap()
        : stats_(std::make_unique<kit::Counters>()),
          tree_(CountingLess{stats_.get()}) {}

    StlPriceThresholdMap(const StlPriceThresholdMap& other)
        : stats_(std::make_unique<kit::Counters>(*other.stats_)),
          tree_(CountingLess{stats_.get()}),
          total_subscribers_(other.total_subscribers_) {
        for (const auto& kv : other.tree_) {
            tree_.emplace(kv.first, kv.second);
        }
    }

    StlPriceThresholdMap& operator=(const StlPriceThresholdMap& other) {
        if (this != &other) {
            StlPriceThresholdMap copy(other);
            *this = std::move(copy);
        }
        return *this;
    }

    StlPriceThresholdMap(StlPriceThresholdMap&& other) noexcept = default;
    StlPriceThresholdMap& operator=(StlPriceThresholdMap&& other) noexcept = default;

    bool insert(const Key& threshold, const Value& sub_id) {
        KIT_COUNT(*stats_, visits);
        auto it = tree_.find(threshold);
        if (it == tree_.end()) {
            KIT_COUNT(*stats_, allocations);
            tree_.emplace(threshold, std::vector<Value>{sub_id});
            ++total_subscribers_;
            return true;
        }
        it->second.push_back(sub_id);
        ++total_subscribers_;
        return true;
    }

    bool erase(const Key& threshold, const Value& sub_id) {
        KIT_COUNT(*stats_, visits);
        auto it = tree_.find(threshold);
        if (it == tree_.end()) {
            return false;
        }
        auto& bucket = it->second;
        for (std::size_t i = 0; i < bucket.size(); ++i) {
            KIT_COUNT(*stats_, visits);
            if (bucket[i] == sub_id) {
                if (i + 1 < bucket.size()) {
                    KIT_COUNT_N(*stats_, moves, bucket.size() - i - 1);
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
        while (it != tree_.begin()) {
            --it;
            KIT_COUNT(*stats_, visits);
            KIT_COUNT(*stats_, comparisons);
            if (!(new_price < it->first)) {
                break;
            }
            for (const Value& sub_id : it->second) {
                KIT_COUNT_EXTRA(*stats_, "returned");
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

    const kit::Counters& stats() const { return *stats_; }
    void reset_stats() { stats_->reset(); }

    static constexpr std::size_t node_bytes() {
        // Approximate libstdc++/libc++ std::map node header (32B) + pair<Key, vector<Value>>
        return 32 + sizeof(std::pair<const Key, std::vector<Value>>);
    }

private:
    // Declared before `tree_` so `stats_` outlives `tree_`'s comparator on destruction.
    mutable std::unique_ptr<kit::Counters> stats_;
    std::map<Key, std::vector<Value>, CountingLess> tree_;
    std::size_t total_subscribers_ = 0;
};

}  // namespace team
