#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "kit/counters.h"

namespace team {

// Core Structure 2 (--impl=main): Hand-built AVL Tree for per-item price thresholds.
// Keys are integer-cent thresholds; duplicate thresholds share an insertion-ordered
// bucket of subscriber IDs. Removing the last subscriber in a bucket deletes the node.
template <class Key = std::int64_t, class Value = std::uint64_t>
class PriceThresholdAVL {
public:
    struct Node {
        Key threshold;
        std::vector<Value> subscribers;
        Node* left = nullptr;
        Node* right = nullptr;
        int height = 1;

        Node(const Key& k, const Value& v) : threshold(k), subscribers{v} {}
        Node(const Key& k, std::vector<Value> subs)
            : threshold(k), subscribers(std::move(subs)) {}
    };

    PriceThresholdAVL() = default;

    ~PriceThresholdAVL() { clear(); }

    PriceThresholdAVL(const PriceThresholdAVL& other)
        : root_(clone_subtree(other.root_)),
          node_count_(other.node_count_),
          total_subscribers_(other.total_subscribers_),
          stats_(other.stats_) {}

    PriceThresholdAVL& operator=(const PriceThresholdAVL& other) {
        if (this != &other) {
            PriceThresholdAVL copy(other);
            swap(copy);
        }
        return *this;
    }

    PriceThresholdAVL(PriceThresholdAVL&& other) noexcept
        : root_(other.root_),
          node_count_(other.node_count_),
          total_subscribers_(other.total_subscribers_),
          stats_(std::move(other.stats_)) {
        other.root_ = nullptr;
        other.node_count_ = 0;
        other.total_subscribers_ = 0;
    }

    PriceThresholdAVL& operator=(PriceThresholdAVL&& other) noexcept {
        if (this != &other) {
            clear();
            root_ = other.root_;
            node_count_ = other.node_count_;
            total_subscribers_ = other.total_subscribers_;
            stats_ = std::move(other.stats_);
            other.root_ = nullptr;
            other.node_count_ = 0;
            other.total_subscribers_ = 0;
        }
        return *this;
    }

    void swap(PriceThresholdAVL& other) noexcept {
        std::swap(root_, other.root_);
        std::swap(node_count_, other.node_count_);
        std::swap(total_subscribers_, other.total_subscribers_);
        std::swap(stats_, other.stats_);
    }

    void clear() {
        destroy_subtree(root_);
        root_ = nullptr;
        node_count_ = 0;
        total_subscribers_ = 0;
    }

    bool insert(const Key& threshold, const Value& sub_id) {
        root_ = insert_impl(root_, threshold, sub_id);
        return true;
    }

    bool erase(const Key& threshold, const Value& sub_id) {
        bool removed = false;
        root_ = erase_impl(root_, threshold, sub_id, removed);
        return removed;
    }

    // Visits every (threshold, sub_id) with new_price < threshold <= old_price,
    // ordered by threshold descending (highest first), then subscription insertion order.
    template <class Visitor>
    void visit_crossed(const Key& new_price, const Key& old_price, Visitor&& visitor) const {
        if (!root_ || !(new_price < old_price)) {
            return;
        }
        visit_crossed_impl(root_, new_price, old_price, visitor);
    }

    bool empty() const { return node_count_ == 0; }
    std::size_t size() const { return node_count_; }
    std::size_t subscriber_count() const { return total_subscribers_; }
    int height() const { return node_height(root_); }

    bool check_invariants() const {
        std::size_t seen_nodes = 0;
        std::size_t seen_subs = 0;
        int computed_h = 0;
        if (!verify_node(root_, nullptr, nullptr, computed_h, seen_nodes, seen_subs)) {
            return false;
        }
        return seen_nodes == node_count_ && seen_subs == total_subscribers_;
    }

    const kit::Counters& stats() const { return stats_; }
    void reset_stats() { stats_.reset(); }

    static constexpr std::size_t node_bytes() { return sizeof(Node); }

private:
    static int node_height(const Node* n) { return n ? n->height : 0; }

    static int balance_factor(const Node* n) {
        return n ? (node_height(n->left) - node_height(n->right)) : 0;
    }

    static void update_height(Node* n) {
        int lh = node_height(n->left);
        int rh = node_height(n->right);
        n->height = 1 + (lh > rh ? lh : rh);
    }

    Node* rotate_right(Node* y) {
        Node* x = y->left;
        Node* t2 = x->right;
        x->right = y;
        y->left = t2;
        update_height(y);
        update_height(x);
        KIT_COUNT_N(stats_, moves, 3);
        KIT_COUNT_EXTRA(stats_, "rotations");
        return x;
    }

    Node* rotate_left(Node* x) {
        Node* y = x->right;
        Node* t2 = y->left;
        y->left = x;
        x->right = t2;
        update_height(x);
        update_height(y);
        KIT_COUNT_N(stats_, moves, 3);
        KIT_COUNT_EXTRA(stats_, "rotations");
        return y;
    }

    Node* rebalance(Node* n) {
        update_height(n);
        int bf = balance_factor(n);
        if (bf > 1) {
            if (balance_factor(n->left) < 0) {
                // LR case
                n->left = rotate_left(n->left);
            }
            // LL case (or second step of LR)
            return rotate_right(n);
        }
        if (bf < -1) {
            if (balance_factor(n->right) > 0) {
                // RL case
                n->right = rotate_right(n->right);
            }
            // RR case (or second step of RL)
            return rotate_left(n);
        }
        return n;
    }

    Node* insert_impl(Node* cur, const Key& threshold, const Value& sub_id) {
        if (!cur) {
            KIT_COUNT(stats_, allocations);
            ++node_count_;
            ++total_subscribers_;
            return new Node(threshold, sub_id);
        }
        KIT_COUNT(stats_, visits);
        KIT_COUNT(stats_, comparisons);
        if (threshold < cur->threshold) {
            cur->left = insert_impl(cur->left, threshold, sub_id);
            return rebalance(cur);
        }
        KIT_COUNT(stats_, comparisons);
        if (cur->threshold < threshold) {
            cur->right = insert_impl(cur->right, threshold, sub_id);
            return rebalance(cur);
        }
        // Duplicate threshold: append to bucket (no tree structural change).
        cur->subscribers.push_back(sub_id);
        ++total_subscribers_;
        return cur;
    }

    // Detaches the minimum node in `cur`'s subtree without deleting it,
    // rebalancing the remaining subtree on the way back up.
    Node* extract_min(Node* cur, Node*& min_node) {
        KIT_COUNT(stats_, visits);
        if (!cur->left) {
            min_node = cur;
            return cur->right;
        }
        cur->left = extract_min(cur->left, min_node);
        return rebalance(cur);
    }

    Node* erase_impl(Node* cur, const Key& threshold, const Value& sub_id, bool& removed) {
        if (!cur) {
            return nullptr;
        }
        KIT_COUNT(stats_, visits);
        KIT_COUNT(stats_, comparisons);
        if (threshold < cur->threshold) {
            cur->left = erase_impl(cur->left, threshold, sub_id, removed);
            return removed ? rebalance(cur) : cur;
        }
        KIT_COUNT(stats_, comparisons);
        if (cur->threshold < threshold) {
            cur->right = erase_impl(cur->right, threshold, sub_id, removed);
            return removed ? rebalance(cur) : cur;
        }

        // Found matching threshold node: locate sub_id in bucket.
        auto& bucket = cur->subscribers;
        for (std::size_t i = 0; i < bucket.size(); ++i) {
            KIT_COUNT(stats_, visits);
            if (bucket[i] == sub_id) {
                if (i + 1 < bucket.size()) {
                    KIT_COUNT_N(stats_, moves, bucket.size() - i - 1);
                }
                bucket.erase(bucket.begin() + static_cast<std::ptrdiff_t>(i));
                --total_subscribers_;
                removed = true;
                break;
            }
        }
        if (!removed || !bucket.empty()) {
            return cur;
        }

        // Bucket is now empty: delete this AVL node.
        if (!cur->left || !cur->right) {
            Node* child = cur->left ? cur->left : cur->right;
            delete cur;
            --node_count_;
            return child;
        }

        // Two-child deletion: replace with in-order successor's key and full bucket.
        Node* succ = nullptr;
        cur->right = extract_min(cur->right, succ);
        cur->threshold = std::move(succ->threshold);
        cur->subscribers = std::move(succ->subscribers);
        KIT_COUNT_N(stats_, moves, 2);
        delete succ;
        --node_count_;
        return rebalance(cur);
    }

    template <class Visitor>
    void visit_crossed_impl(const Node* node,
                            const Key& new_price,
                            const Key& old_price,
                            Visitor& visitor) const {
        if (!node) {
            return;
        }
        KIT_COUNT(stats_, visits);

        // Right subtree has keys > node->threshold; only visit if node->threshold < old_price.
        KIT_COUNT(stats_, comparisons);
        bool lt_old = (node->threshold < old_price);
        if (lt_old) {
            visit_crossed_impl(node->right, new_price, old_price, visitor);
        }

        // Current node and left subtree only matter if node->threshold > new_price.
        KIT_COUNT(stats_, comparisons);
        bool gt_new = (new_price < node->threshold);
        if (gt_new) {
            bool le_old = lt_old;
            if (!le_old) {
                KIT_COUNT(stats_, comparisons);
                le_old = !(old_price < node->threshold);
            }
            if (le_old) {
                for (const Value& sub_id : node->subscribers) {
                    KIT_COUNT_EXTRA(stats_, "returned");
                    visitor(node->threshold, sub_id);
                }
            }
            visit_crossed_impl(node->left, new_price, old_price, visitor);
        }
    }

    static void destroy_subtree(Node* n) {
        if (!n) {
            return;
        }
        destroy_subtree(n->left);
        destroy_subtree(n->right);
        delete n;
    }

    static Node* clone_subtree(const Node* n) {
        if (!n) {
            return nullptr;
        }
        Node* copy = new Node(n->threshold, n->subscribers);
        copy->height = n->height;
        copy->left = clone_subtree(n->left);
        copy->right = clone_subtree(n->right);
        return copy;
    }

    static bool verify_node(const Node* n,
                            const Key* min_key,
                            const Key* max_key,
                            int& out_height,
                            std::size_t& seen_nodes,
                            std::size_t& seen_subs) {
        if (!n) {
            out_height = 0;
            return true;
        }
        if (n->subscribers.empty()) {
            return false;
        }
        if (min_key && !(*min_key < n->threshold)) {
            return false;
        }
        if (max_key && !(n->threshold < *max_key)) {
            return false;
        }
        int lh = 0;
        int rh = 0;
        if (!verify_node(n->left, min_key, &n->threshold, lh, seen_nodes, seen_subs)) {
            return false;
        }
        if (!verify_node(n->right, &n->threshold, max_key, rh, seen_nodes, seen_subs)) {
            return false;
        }
        int expected_h = 1 + (lh > rh ? lh : rh);
        if (n->height != expected_h) {
            return false;
        }
        int bf = lh - rh;
        if (bf < -1 || bf > 1) {
            return false;
        }
        out_height = expected_h;
        ++seen_nodes;
        seen_subs += n->subscribers.size();
        return true;
    }

    Node* root_ = nullptr;
    std::size_t node_count_ = 0;
    std::size_t total_subscribers_ = 0;
    mutable kit::Counters stats_;
};

}  // namespace team
