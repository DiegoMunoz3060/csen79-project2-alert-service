#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "kit/counters.h"

namespace team {

// STL baseline for Core Structure 1 (Hierarchical Topic Store).
// Maps exact topic paths to their direct subscribers and enumerates all
// matching slash-delimited prefixes from shallowest (depth 1) to deepest.
template <class SubId = std::uint64_t>
class StlTopicStore {
public:
    void add_subscription(const std::string& topic, const SubId& sub_id) {
        KIT_COUNT(stats_, visits);
        auto& vec = topics_[topic];
        vec.push_back(sub_id);
        ++total_subscribers_;
    }

    bool remove_subscription(const std::string& topic, const SubId& sub_id) {
        KIT_COUNT(stats_, visits);
        auto it = topics_.find(topic);
        KIT_COUNT(stats_, comparisons);
        if (it == topics_.end()) {
            return false;
        }
        auto& vec = it->second;
        for (std::size_t i = 0; i < vec.size(); ++i) {
            KIT_COUNT(stats_, visits);
            if (vec[i] == sub_id) {
                if (i + 1 < vec.size()) {
                    vec[i] = vec.back();
                    KIT_COUNT(stats_, moves);
                }
                vec.pop_back();
                --total_subscribers_;
                return true;
            }
        }
        return false;
    }

    // Calls visitor(prefix_depth, sub_id) for every direct subscriber at every
    // slash-delimited prefix of `topic`, shallowest to deepest.
    template <class Visitor>
    void visit_matches(const std::string& topic, Visitor&& visitor) const {
        std::size_t depth = 0;
        std::size_t pos = 0;
        while (pos < topic.size()) {
            std::size_t slash = topic.find('/', pos);
            std::size_t end = (slash == std::string::npos) ? topic.size() : slash;
            ++depth;
            std::string prefix = topic.substr(0, end);
            KIT_COUNT(stats_, visits);
            auto it = topics_.find(prefix);
            KIT_COUNT(stats_, comparisons);
            if (it != topics_.end()) {
                for (const SubId& sub_id : it->second) {
                    KIT_COUNT_EXTRA(stats_, "candidates");
                    visitor(depth, sub_id);
                }
            }
            if (slash == std::string::npos) {
                break;
            }
            pos = slash + 1;
        }
    }

    std::size_t subscriber_count() const { return total_subscribers_; }
    const kit::Counters& stats() const { return stats_; }
    void reset_stats() { stats_.reset(); }

private:
    std::unordered_map<std::string, std::vector<SubId>> topics_;
    std::size_t total_subscribers_ = 0;
    mutable kit::Counters stats_;
};

}  // namespace team
