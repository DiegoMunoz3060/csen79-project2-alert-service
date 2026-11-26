#include <cstdint>
#include <limits>
#include <random>
#include <utility>
#include <vector>

#include "doctest/doctest.h"
#include "team/price_threshold_avl.h"
#include "team/sorted_threshold_array.h"
#include "team/stl_price_threshold.h"

namespace {

using HitList = std::vector<std::pair<std::int64_t, std::uint64_t>>;

template <class Index>
HitList collect_crossed(const Index& idx, std::int64_t new_price, std::int64_t old_price) {
    HitList hits;
    idx.visit_crossed(new_price, old_price, [&](std::int64_t threshold, std::uint64_t sub_id) {
        hits.emplace_back(threshold, sub_id);
    });
    return hits;
}

template <class Index>
void run_core_behavior_suite() {
    Index idx;

    // 1. Empty structure
    CHECK(idx.empty());
    CHECK(idx.size() == 0);
    CHECK(idx.subscriber_count() == 0);
    CHECK(idx.check_invariants());
    CHECK_FALSE(idx.erase(50000, 1));
    CHECK(collect_crossed(idx, 10000, 90000).empty());

    // 2. Single element: insert, range query, erase, and reuse after empty
    CHECK(idx.insert(60000, 10));
    CHECK_FALSE(idx.empty());
    CHECK(idx.size() == 1);
    CHECK(idx.subscriber_count() == 1);
    CHECK(idx.check_invariants());

    // Boundary checks: exclusive lower bound (new < t), inclusive upper bound (t <= old)
    CHECK(collect_crossed(idx, 60000, 70000).empty());             // landed exactly on threshold
    CHECK(collect_crossed(idx, 60000, 60000).empty());             // flat
    CHECK(collect_crossed(idx, 70000, 50000).empty());             // rising
    CHECK(collect_crossed(idx, 59999, 60000) == HitList{{60000, 10}});  // inclusive upper
    CHECK(collect_crossed(idx, 50000, std::numeric_limits<std::int64_t>::max()) ==
          HitList{{60000, 10}});

    // Erase wrong subscriber ID at existing threshold does nothing
    CHECK_FALSE(idx.erase(60000, 999));
    CHECK(idx.size() == 1);
    CHECK(idx.check_invariants());

    // Remove the only element
    CHECK(idx.erase(60000, 10));
    CHECK(idx.empty());
    CHECK(idx.size() == 0);
    CHECK(idx.subscriber_count() == 0);
    CHECK(idx.check_invariants());

    // 3. Reuse after empty + duplicate thresholds (remove first, middle, last)
    idx.insert(50000, 1);
    idx.insert(50000, 2);
    idx.insert(50000, 3);
    idx.insert(60000, 4);
    idx.insert(40000, 5);
    CHECK(idx.size() == 3);
    CHECK(idx.subscriber_count() == 5);
    CHECK(idx.check_invariants());

    // Highest threshold first, insertion order within each threshold
    CHECK(collect_crossed(idx, 30000, 70000) ==
          HitList{{60000, 4}, {50000, 1}, {50000, 2}, {50000, 3}, {40000, 5}});

    // Remove first, last, and remaining subscriber in duplicate bucket
    CHECK(idx.erase(50000, 1));
    CHECK(idx.size() == 3);
    CHECK(idx.check_invariants());
    CHECK(collect_crossed(idx, 45000, 55000) == HitList{{50000, 2}, {50000, 3}});

    CHECK(idx.erase(50000, 3));
    CHECK(idx.size() == 3);
    CHECK(idx.check_invariants());
    CHECK(collect_crossed(idx, 45000, 55000) == HitList{{50000, 2}});

    CHECK(idx.erase(50000, 2));  // empties bucket -> removes key
    CHECK(idx.size() == 2);
    CHECK(idx.check_invariants());
    CHECK(collect_crossed(idx, 45000, 55000).empty());

    // Remove smallest (first) and largest (last) keys
    CHECK(idx.erase(40000, 5));
    CHECK(idx.check_invariants());
    CHECK(idx.erase(60000, 4));
    CHECK(idx.empty());
    CHECK(idx.check_invariants());
}

}  // namespace

TEST_CASE("Structure 2 core behavior holds for AVL, SortedArray, and STL map") {
    SUBCASE("PriceThresholdAVL") { run_core_behavior_suite<team::PriceThresholdAVL<>>(); }
    SUBCASE("SortedThresholdArray") { run_core_behavior_suite<team::SortedThresholdArray<>>(); }
    SUBCASE("StlPriceThresholdMap") { run_core_behavior_suite<team::StlPriceThresholdMap<>>(); }
}

TEST_CASE("PriceThresholdAVL handles all four rotation cases (LL, RR, LR, RL)") {
    SUBCASE("LL rotation") {
        team::PriceThresholdAVL<> avl;
        avl.insert(300, 1);
        CHECK(avl.check_invariants());
        avl.insert(200, 2);
        CHECK(avl.check_invariants());
        avl.insert(100, 3);
        CHECK(avl.check_invariants());
        CHECK(avl.height() == 2);
        CHECK(collect_crossed(avl, 0, 500) == HitList{{300, 1}, {200, 2}, {100, 3}});
#if KIT_COUNTERS_ENABLED
        CHECK(avl.stats().extra.at("rotations") == 1);
#endif
    }

    SUBCASE("RR rotation") {
        team::PriceThresholdAVL<> avl;
        avl.insert(100, 1);
        avl.insert(200, 2);
        avl.insert(300, 3);
        CHECK(avl.check_invariants());
        CHECK(avl.height() == 2);
        CHECK(collect_crossed(avl, 0, 500) == HitList{{300, 3}, {200, 2}, {100, 1}});
#if KIT_COUNTERS_ENABLED
        CHECK(avl.stats().extra.at("rotations") == 1);
#endif
    }

    SUBCASE("LR rotation") {
        team::PriceThresholdAVL<> avl;
        avl.insert(300, 1);
        avl.insert(100, 2);
        avl.insert(200, 3);
        CHECK(avl.check_invariants());
        CHECK(avl.height() == 2);
        CHECK(collect_crossed(avl, 0, 500) == HitList{{300, 1}, {200, 3}, {100, 2}});
#if KIT_COUNTERS_ENABLED
        CHECK(avl.stats().extra.at("rotations") == 2);
#endif
    }

    SUBCASE("RL rotation") {
        team::PriceThresholdAVL<> avl;
        avl.insert(100, 1);
        avl.insert(300, 2);
        avl.insert(200, 3);
        CHECK(avl.check_invariants());
        CHECK(avl.height() == 2);
        CHECK(collect_crossed(avl, 0, 500) == HitList{{300, 2}, {200, 3}, {100, 1}});
#if KIT_COUNTERS_ENABLED
        CHECK(avl.stats().extra.at("rotations") == 2);
#endif
    }
}

TEST_CASE("PriceThresholdAVL handles root deletion and two-child bucket replacement") {
    team::PriceThresholdAVL<> avl;
    // Build a multi-level tree where the root has two children and its successor has
    // multiple subscribers AND a right child.
    avl.insert(500, 1);
    avl.insert(300, 2);
    avl.insert(700, 3);
    avl.insert(200, 4);
    avl.insert(400, 5);
    avl.insert(600, 6);
    avl.insert(600, 60);  // duplicate subscriber on in-order successor of 500
    avl.insert(800, 7);
    avl.insert(650, 8);   // right child of successor 600
    REQUIRE(avl.check_invariants());

    // Delete root (500): successor (600) with bucket {6, 60} must move intact, and 650 stays linked.
    CHECK(avl.erase(500, 1));
    CHECK(avl.check_invariants());
    CHECK(avl.size() == 7);
    CHECK(avl.subscriber_count() == 8);
    CHECK(collect_crossed(avl, 550, 660) == HitList{{650, 8}, {600, 6}, {600, 60}});

    // Erase the moved successor's bucket elements one by one.
    CHECK(avl.erase(600, 6));
    CHECK(avl.check_invariants());
    CHECK(avl.erase(600, 60));
    CHECK(avl.check_invariants());
    CHECK(collect_crossed(avl, 0, 1000) ==
          HitList{{800, 7}, {700, 3}, {650, 8}, {400, 5}, {300, 2}, {200, 4}});

    // Delete all remaining nodes in arbitrary order, checking invariants every step.
    for (auto [k, id] : std::vector<std::pair<std::int64_t, std::uint64_t>>{
             {700, 3}, {300, 2}, {650, 8}, {200, 4}, {800, 7}, {400, 5}}) {
        CHECK(avl.erase(k, id));
        CHECK(avl.check_invariants());
    }
    CHECK(avl.empty());
}

TEST_CASE("PriceThresholdAVL copy and move semantics are leak-free and independent") {
    team::PriceThresholdAVL<> a;
    for (int i = 1; i <= 25; ++i) {
        a.insert(i * 100, static_cast<std::uint64_t>(i));
    }
    REQUIRE(a.check_invariants());

    team::PriceThresholdAVL<> b(a);  // copy ctor
    CHECK(b.check_invariants());
    CHECK(collect_crossed(a, 0, 5000) == collect_crossed(b, 0, 5000));

    b.erase(1000, 10);
    CHECK(a.size() == 25);
    CHECK(b.size() == 24);
    CHECK(a.check_invariants());
    CHECK(b.check_invariants());

    team::PriceThresholdAVL<> c(std::move(b));  // move ctor
    CHECK(c.size() == 24);
    CHECK(b.empty());
    CHECK(c.check_invariants());
    CHECK(b.check_invariants());

    team::PriceThresholdAVL<> d;
    d = a;  // copy assign
    CHECK(d.size() == 25);
    CHECK(d.check_invariants());

    d = std::move(c);  // move assign
    CHECK(d.size() == 24);
    CHECK(c.empty());
    CHECK(d.check_invariants());
}

TEST_CASE("Randomized differential test: AVL, SortedArray, and STL map always agree") {
    team::PriceThresholdAVL<> avl;
    team::SortedThresholdArray<> arr;
    team::StlPriceThresholdMap<> stl;

    std::mt19937_64 rng(42);
    std::uniform_int_distribution<std::int64_t> price_dist(100, 200);
    std::vector<std::pair<std::int64_t, std::uint64_t>> live;
    std::uint64_t next_id = 1;

    for (int step = 0; step < 500; ++step) {
        int action = static_cast<int>(rng() % 10);
        if (action < 5 || live.empty()) {
            std::int64_t t = price_dist(rng);
            std::uint64_t id = next_id++;
            avl.insert(t, id);
            arr.insert(t, id);
            stl.insert(t, id);
            live.emplace_back(t, id);
        } else if (action < 8) {
            std::size_t idx = static_cast<std::size_t>(rng() % live.size());
            auto [t, id] = live[idx];
            live[idx] = live.back();
            live.pop_back();
            CHECK(avl.erase(t, id));
            CHECK(arr.erase(t, id));
            CHECK(stl.erase(t, id));
        } else {
            std::int64_t p1 = price_dist(rng);
            std::int64_t p2 = price_dist(rng);
            if (p1 > p2) std::swap(p1, p2);
            HitList h_avl = collect_crossed(avl, p1, p2);
            HitList h_arr = collect_crossed(arr, p1, p2);
            HitList h_stl = collect_crossed(stl, p1, p2);
            REQUIRE(h_avl == h_stl);
            REQUIRE(h_arr == h_stl);
        }
        REQUIRE(avl.check_invariants());
        REQUIRE(arr.check_invariants());
        REQUIRE(stl.check_invariants());
        REQUIRE(avl.size() == stl.size());
        REQUIRE(arr.size() == stl.size());
        REQUIRE(avl.subscriber_count() == stl.subscriber_count());
        REQUIRE(arr.subscriber_count() == stl.subscriber_count());
    }

    // Drain everything completely.
    for (auto [t, id] : live) {
        CHECK(avl.erase(t, id));
        CHECK(arr.erase(t, id));
        CHECK(stl.erase(t, id));
        REQUIRE(avl.check_invariants());
        REQUIRE(arr.check_invariants());
    }
    CHECK(avl.empty());
    CHECK(arr.empty());
    CHECK(stl.empty());
}

TEST_CASE("PriceThresholdAVL stays balanced on strictly decreasing M4 worst-case input") {
    team::PriceThresholdAVL<> avl;
    for (int i = 1000; i >= 1; --i) {
        avl.insert(static_cast<std::int64_t>(i) * 10, static_cast<std::uint64_t>(1001 - i));
    }
    CHECK(avl.size() == 1000);
    CHECK(avl.check_invariants());
    // AVL height bound: <= 1.44 * log2(1002) < 15
    CHECK(avl.height() <= 14);
#if KIT_COUNTERS_ENABLED
    CHECK(avl.stats().comparisons > 0);
    CHECK(avl.stats().allocations == 1000);
    CHECK(avl.stats().extra.at("rotations") > 0);
#endif
    avl.reset_stats();
    CHECK(avl.stats().comparisons == 0);
    CHECK(avl.stats().extra.empty());
}
