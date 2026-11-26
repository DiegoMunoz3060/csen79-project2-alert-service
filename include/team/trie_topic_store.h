#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>

#include "kit/counters.h"
#include "team/trie.h"

namespace team {

// Adapter wrapping Structures Lead's customTrie::trie<T> behind AlertService's
// TopicStore interface. Stores SubHandle slots in a std::deque so pointers held
// inside customTrie::trie never invalidate on growth.
template <class SubId = std::uint64_t>
class TrieTopicStore {
public:
    TrieTopicStore() = default;
    TrieTopicStore(const TrieTopicStore&) = delete;
    TrieTopicStore& operator=(const TrieTopicStore&) = delete;

    void add_subscription(const std::string& topic, const SubId& sub_id) {
        KIT_COUNT(stats_, visits);
        std::size_t node_id = trie_.ensureTopic(topic.c_str());
        std::size_t depth = count_segments(topic);
        handles_.push_back(SubHandle{sub_id, depth, node_id});
        SubHandle* slot = &handles_.back();
        by_sub_id_.emplace(sub_id, slot);
        trie_.addSubscription(node_id, *slot);
        ++total_subscribers_;
    }

    bool remove_subscription(const std::string& topic, const SubId& sub_id) {
        (void)topic;
        KIT_COUNT(stats_, visits);
        auto it = by_sub_id_.find(sub_id);
        KIT_COUNT(stats_, comparisons);
        if (it == by_sub_id_.end()) {
            return false;
        }
        SubHandle* slot = it->second;
        bool removed = trie_.removeSubscription(slot->node_id, *slot);
        by_sub_id_.erase(it);
        if (removed) {
            KIT_COUNT(stats_, moves);
            --total_subscribers_;
        }
        return removed;
    }

    // Calls visitor(prefix_depth, sub_id) for every direct subscriber at every
    // existing slash-delimited prefix of `topic`, shallowest to deepest.
    template <class Visitor>
    void visit_matches(const std::string& topic, Visitor&& visitor) const {
        match_buf_.clear();
        KIT_COUNT(stats_, visits);
        trie_.visitMatches(topic.c_str(), match_buf_);
        for (std::size_t i = 0; i < match_buf_.size(); ++i) {
            const SubHandle* h = match_buf_[i];
            KIT_COUNT_EXTRA(stats_, "candidates");
            visitor(h->depth, h->id);
        }
    }

    std::size_t subscriber_count() const { return total_subscribers_; }
    const kit::Counters& stats() const { return stats_; }
    void reset_stats() { stats_.reset(); }

private:
    struct SubHandle {
        SubId id;
        std::size_t depth;
        std::size_t node_id;
    };

    static std::size_t count_segments(const std::string& topic) {
        if (topic.empty()) {
            return 0;
        }
        std::size_t count = 1;
        for (char ch : topic) {
            if (ch == '/') {
                ++count;
            }
        }
        return count;
    }

    // IMPORTANT: `handles_` must be declared before `trie_` and `match_buf_` so
    // `trie_` is destroyed before the SubHandle objects its raw pointers refer to.
    std::deque<SubHandle> handles_;
    std::unordered_map<SubId, SubHandle*> by_sub_id_;
    customTrie::trie<SubHandle> trie_;
    mutable customTrie::vector<SubHandle*> match_buf_;
    std::size_t total_subscribers_ = 0;
    mutable kit::Counters stats_;
};

}  // namespace team
