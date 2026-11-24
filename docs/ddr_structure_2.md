# Design Decision Record 2: Price Threshold Index

- **Owner:** Madhav Korimerla (`@Krishy2007`) — App Lead
- **Component:** Core Structure 2 (`include/team/price_threshold_avl.h`)
- **Chosen Structure (`--impl=main`):** Hand-built AVL Tree with per-threshold subscriber buckets (`PriceThresholdAVL<Key, Value>`)
- **Alternative Structure (`--impl=alt`):** Sorted Dynamic Array with binary search (`SortedThresholdArray<Key, Value>`)
- **STL Baseline (`--impl=stl`):** Instrumented `std::map<Key, std::vector<Value>>` wrapper (`StlPriceThresholdMap<Key, Value>`)
- **Status:** Parts 1–4 drafted and committed **prior to benchmarking**; Parts 5–6 pending `M1`–`M5` measurements.

---

## Part 1: Problem & Requirements

### 1.1 Functional Requirement
In the Alert Service (Project 2), users register price-drop alerts on exact items via:

```text
SUB <user> <item> below <price>
UNSUB <user> <item>
PRICE <item> <new_price>
```

Each item maintains its own price-threshold index and its most recent known price (`old_price`). On a `PRICE <item> <new_price>` event:
- If `new_price >= old_price` (flat or rising), **no alerts** are triggered.
- If `new_price < old_price` (downward movement; the first-ever price for an item counts as a drop from $+\infty$), the index must find **every active subscription** whose threshold satisfies:
  $$\text{new\_price} < \text{threshold} \le \text{old\_price}$$
- Matching alerts must be returned **without scanning all thresholds registered on that item**.
- Alert output ordering for a single `PRICE` command is **highest threshold first**, breaking ties by subscription creation order (oldest first).
- Duplicate thresholds on the same item must be supported (multiple users watching the same price point).
- `UNSUB <user> <item>` must remove only that user's subscription from its threshold bucket without scanning every subscriber on the item, and delete the threshold key when its last subscriber leaves.

### 1.2 Data Representation & Invariants
- **Exact currency representation:** All prices and thresholds are stored as signed 64-bit integer cents (`std::int64_t`, e.g., `579.00` $\rightarrow$ `57900`), never `double`.
- **Per-threshold buckets:** Each distinct threshold stores a dynamic list of subscriber IDs in insertion order.
- **Structural invariants (`check_invariants()`):**
  1. Strict BST ordering: for every node, all keys in `left` are strictly less than `node->threshold` and all keys in `right` are strictly greater.
  2. Exact height maintenance: `node->height == 1 + max(height(left), height(right))` (with `height(nullptr) == 0`).
  3. AVL balance property: `balance_factor(node) = height(left) - height(right)` is in `{-1, 0, +1}` for every node.
  4. No empty buckets: every node in the tree contains at least one active subscriber ID.
  5. Exact counters: tracked `node_count_` and `total_subscribers_` match a full traversal of the tree.

---

## Part 2: Workload Analysis & Operation Frequencies

Operation mixes are derived directly from the starter kit's seeded workload generator (`tools/gen_alerts.cpp`), which inserts `N = 100,000` subscriptions (`--subs 100000`) followed by `10,000` events (`--events 10000`):

| Workload | Subscription Distribution (`N = 100,000`) | Event Distribution (`10,000` events) | Stress Profile on Price Index |
| :--- | :--- | :--- | :--- |
| **W1 (Realistic)** | ~15% (~15,000) `below` thresholds across 250 Zipf-distributed items (~60 avg/item, hotter items up to a few thousand) | 70% `PUB`, 15% `PRICE` (~1,500), 10% `TICK`, 3% `UNSUB`, 2% `INBOX`/`HISTORY` | Moderate tree sizes; mixed updates and range queries across many items |
| **W2 (Hot Topic)** | 50% of users on `news/world`; ~15% of remaining subs are `below` thresholds | Same event mix as W1 (plus hot `news/world/s0` `PUB`s) | Light-to-moderate load on price index |
| **W3 (Price Storm)** | **95% (~95,000)** `below` thresholds on a **single item** (`items/phones/item0`), thresholds uniform in `$100.00`–`$999.99` (`10,000`–`99,999` cents) | **80% (~8,000)** step-by-step `PRICE` drops on `item0` (`1`–`20` cents each from `$1000.00`), 5% other `PRICE`, 10% `TICK`, **3% (~300)** `UNSUB`, 2% `INBOX`/`HISTORY` | **Primary bottleneck:** ~95,000 thresholds in one index with thousands of narrow downward range queries and live `UNSUB`s |
| **M4 (Worst Case)** | `N = 100,000` `below` thresholds on one item inserted in **strictly decreasing** (or strictly increasing) price order, followed by `PRICE` drops and `UNSUB`s | Targeted adversarial sequence | Maximizes element shifting in sorted arrays ($O(N^2)$) and breaks unbalanced BSTs, testing whether AVL stays $O(\log m)$ |

### Expected W3 Tree Shape (`N = 100,000`)
In **W3**, ~95,000 thresholds on `items/phones/item0` are drawn uniformly from $R = 90{,}000$ integer-cent values (`10000` to `99999`):
- **Expected distinct thresholds ($m$):**
  $$m \approx R \left(1 - \left(1 - \frac{1}{R}\right)^{95{,}000}\right) \approx 90{,}000 \left(1 - e^{-95/90}\right) \approx 58{,}680$$
- **Average subscribers per bucket ($b$):** $95{,}000 / 58{,}680 \approx 1.62$.
- **Expected crossed thresholds per W3 `PRICE` step** (average drop $\Delta = 10.5$ cents):
  - Distinct crossed keys: $k \approx 10.5 \times \frac{58{,}680}{90{,}000} \approx 6.8$ nodes
  - Returned subscribers: $a \approx 10.5 \times \frac{95{,}000}{90{,}000} \approx 11.1$ alerts

---

## Part 3: Candidate Structures & Trade-offs

Let $m$ be the number of distinct thresholds on an item, $b$ the size of a single threshold's subscriber bucket, $k$ the number of distinct thresholds crossed by a `PRICE` event, and $a$ the total subscriber alerts emitted.

### 3.1 Candidates Considered
1. **Chosen (`--impl=main`) — Hand-built AVL Tree (`PriceThresholdAVL`):**
   - Self-balancing binary search tree storing `(threshold, vector<SubId>)` per node, enforcing height balance $\in [-1, 1]$ via `LL`, `RR`, `LR`, and `RL` rotations.
   - `visit_crossed(new_price, old_price, fn)` uses **pruned reverse in-order traversal** (right $\rightarrow$ node $\rightarrow$ left), visiting right only when `node->threshold < old_price` and left only when `node->threshold > new_price`. This naturally emits crossed thresholds **highest-first** with zero extra sorting.
2. **Alternative (`--impl=alt`) — Sorted Dynamic Array (`SortedThresholdArray`):**
   - Contiguous growable array of `(threshold, vector<SubId>)` kept sorted by `threshold`.
   - Uses binary search (`lower_bound` / `upper_bound`) to locate insertion points, deletion targets, and range-query endpoints, then iterates backward from `old_price` down to `new_price` for highest-first output.
   - Very cache-friendly for range queries and compact in memory, **but** inserting a new distinct threshold or erasing an emptied threshold requires shifting up to $O(m)$ array entries.
3. **STL Baseline (`--impl=stl`) — Instrumented `std::map` Wrapper (`StlPriceThresholdMap`):**
   - Wraps `std::map<std::int64_t, std::vector<SubId>>` (Red-Black Tree) behind the exact same method interface as `PriceThresholdAVL`.
   - Uses `upper_bound(old_price)` and decrements backward while `key > new_price`.

### 3.2 Asymptotic Complexity Comparison

| Operation | AVL Tree (`main`) | Sorted Dynamic Array (`alt`) | `std::map` (`stl`, RB-Tree) |
| :--- | :--- | :--- | :--- |
| **`SUB` (insert)** | $O(\log m)$ worst-case | $O(\log m)$ search + **$O(m)$ shifts** | $O(\log m)$ worst-case |
| **`UNSUB` (erase)** | $O(\log m + b)$ worst-case | $O(\log m + b)$ + **$O(m)$ shifts** when bucket empties | $O(\log m + b)$ worst-case |
| **`PRICE` (`visit_crossed`)** | $O(\log m + k + a)$ | $O(\log m + k + a)$ *(contiguous scan)* | $O(\log m + k + a)$ |
| **Worst-case tree height / build (`N` sorted inserts)** | Height $\le 1.44 \log_2(m+2)$; $O(N \log N)$ | $O(N^2)$ shifts (decreasing order) | Height $\le 2 \log_2(m+1)$; $O(N \log N)$ |
| **Cache locality** | Lower (pointer chasing) | **High** (contiguous memory) | Lower (pointer chasing) |

### 3.3 Estimated Memory Footprint — M5 (64-bit Architecture)
Assuming `Key = std::int64_t` (8 bytes), `Value = std::uint64_t` (8 bytes), and `std::vector<Value>` header = 24 bytes (excluding the shared per-bucket heap buffer of subscriber IDs, which is identical across all three designs):

| Structure | Struct Fields per Distinct Threshold | `sizeof(Node/Entry)` | Estimated Total Bytes / Distinct Threshold (incl. allocator/capacity overhead) |
| :--- | :--- | :---: | :--- |
| **AVL Tree (`main`)** | `threshold` (8B) + `subscribers` (24B) + `left*` (8B) + `right*` (8B) + `height` (4B + 4B pad) | **56 B** | **~72 B** (+ ~16 B heap allocator header per node) |
| **Sorted Array (`alt`)** | `threshold` (8B) + `subscribers` (24B) | **32 B** | **~32–64 B** (contiguous buffer, $1\times$–$2\times$ geometric growth factor, avg ~48 B) |
| **`std::map` (`stl`)** | RB header (`color` + `parent*` + `left*` + `right*` = 32B) + `pair<const int64_t, vector>` (32B) | **64 B** | **~80 B** (+ ~16 B heap allocator header per node) |

*(Exact `sizeof` values on the SCU ECC compiler will be verified and recorded in Part 5.)*

### 3.4 Rationale for Choosing AVL Tree
Although a Sorted Dynamic Array offers superior cache locality on pure range scans and smaller per-entry metadata (~32 B vs. 56 B), the Alert Service must handle large volumes of dynamic `SUB` and `UNSUB` commands alongside `PRICE` updates—especially in **W3** (`95,000` thresholds on one item) and **M4** (adversarial insertion order). An AVL Tree guarantees $O(\log m)$ worst-case height ($\le 24$ at $m = 100{,}000$), avoiding quadratic $O(m)$ shifting costs while maintaining optimal $O(\log m + k + a)$ range queries. Compared to `std::map`, a hand-built AVL tree also eliminates the `parent*` pointer per node (saving 8 B/node) and enforces tighter height balance ($1.44 \log_2 m$ vs. $2 \log_2 m$), which should slightly reduce lookup depth on read-heavy price storms.

---

## Part 4: Quantitative Predictions at `N = 100,000` (Pre-Measurement)

All predictions below are calculated analytically **before** running benchmarks on the target machine.

### 4.1 W3 Full Replay (`--op replay`: 95,000 `SUB`s + 8,000 `PRICE`s + ~300 `UNSUB`s on `item0`)
Using $m \approx 58{,}680$ distinct thresholds on `item0` and $k \approx 6.8$ crossed nodes per `PRICE`:

- **AVL Tree (`main`):**
  - 95,000 inserts into a tree growing to $m \approx 58{,}680$:
    $$\approx 95{,}000 \times \log_2(58{,}680 / e) \approx 95{,}000 \times 14.4 \approx 1.37 \times 10^6 \text{ comparisons}$$
    (plus ~35,000–45,000 single rotations during insertion).
  - 8,000 `PRICE` queries:
    $$\approx 8{,}000 \times \left(\log_2(58{,}680) + 2k\right) \approx 8{,}000 \times 30 \approx 2.4 \times 10^5 \text{ node visits/comparisons}$$
  - ~300 `UNSUB`s: $\approx 300 \times 16 \approx 4{,}800$ comparisons.
  - **Predicted Total Dominant Operations (`main`):** $\approx \mathbf{1.6 \times 10^6}$ comparisons/visits.

- **Sorted Dynamic Array (`alt`):**
  - Binary searches across 95,000 `SUB`s: $\approx 1.4 \times 10^6$ comparisons (comparable to AVL).
  - **Array shifts (`moves`) on 58,680 unique key insertions** (each random insert shifts $i/2$ entries on average):
    $$\sum_{i=1}^{58{,}680} \frac{i}{2} \approx \frac{58{,}680^2}{4} \approx \mathbf{8.61 \times 10^8} \text{ entry moves}$$
  - 8,000 `PRICE` queries: $\approx 8{,}000 \times (\log_2(58{,}680) + 6.8) \approx 1.8 \times 10^5$ comparisons/visits.
  - ~300 `UNSUB`s (~185 empty a bucket, shifting ~29,340 entries on average): $\approx 5.4 \times 10^6$ entry moves.
  - **Predicted Total Dominant Operations (`alt`):** $\approx \mathbf{8.66 \times 10^8}$ moves + $1.6 \times 10^6$ comparisons.

- **Predicted Ratio (`alt` / `main`) on W3 `replay` at `N = 100,000`:**
  - **Operation-count ratio:** $\frac{8.68 \times 10^8}{1.6 \times 10^6} \approx \mathbf{540\times}$ fewer dominant operations for AVL.
  - **Wall-clock time ratio:** Because contiguous 32-byte `Entry` moves exploit sequential memory bandwidth and hardware prefetching (whereas AVL pointer hops incur L2/L3 cache misses), we predict the real wall-clock speedup will be smaller than the raw op-count ratio—roughly **15x–50x faster** for AVL on W3 `replay`.

### 4.2 W3 Events-Only (`--op events`: post-`SUB` 8,000 `PRICE`s + ~300 `UNSUB`s)
- On pure `PRICE` queries alone, Sorted Array's contiguous memory scan should match or slightly beat AVL (~0.7x–1.0x wall-clock time).
- However, the ~300 `UNSUB` commands in W3 trigger an estimated ~5.4M entry shifts in Sorted Array vs. ~4,800 comparisons in AVL.
- **Predicted Ratio (`alt` / `main`) on W3 `events`:** AVL should still win overall by roughly **2x–10x** wall-clock time (and **~20x** fewer total operations), driven primarily by `UNSUB` shifting.

### 4.3 AVL Tree (`main`) vs. `std::map` (`stl`)
- Both have identical $O(\log m)$ insert/erase and $O(\log m + k + a)$ range query complexity.
- **Predicted Ratio (`stl` / `main`):** **~0.8x–1.3x** wall-clock time (asymptotically **1.0x**). We expect AVL to record slightly fewer comparisons/visits on W3 `PRICE` queries (tighter height bound) and ~10% less node memory (`56 B` vs. `64 B`), while `std::map` may perform slightly fewer rebalancing rotations during heavy insert/erase phases.

### 4.4 M4 Worst-Case Prediction (100,000 Strictly Decreasing Threshold Inserts)
- **Sorted Dynamic Array (`alt`):** Every new threshold inserts at index `0`, shifting all $i$ existing elements:
  $$\sum_{i=0}^{99{,}999} i \approx \mathbf{5.0 \times 10^9} \text{ entry moves } (O(N^2))$$
- **AVL Tree (`main`):** Self-balances on every insert, maintaining height $\le 24$:
  $$\approx 100{,}000 \times 16.6 \approx \mathbf{1.66 \times 10^6} \text{ comparisons } (O(N \log N))$$
- **Predicted Ratio (`alt` / `main`) on M4:** **~3,000x** fewer dominant operations (**100x+** wall-clock speedup).

---

## Part 5: Measured Evidence (`M1`–`M5`)

> **Status: Pending.** Populate this section after running `bench` (Release `-O2`, `--quiet`, median of 5 runs, fixed seeds) and generating plots in `results/plots/`.

### 5.1 Test Environment & Seeds
- **Machine / Compiler:** `TODO (e.g., SCU ECC Linux x86_64, g++ -O2 -std=c++17)`
- **Seeds & Build Flags:** `TODO (record exact seeds from gen_alerts and KIT_BUILD_FLAGS)`
- **Raw CSV:** `results/results.csv`

### 5.2 Summary Results (`M1`–`M5`)

| Metric | Workload / Size | AVL Tree (`main`) | Sorted Array (`alt`) | `std::map` (`stl`) | Measured Ratio (`alt / main`) |
| :--- | :--- | :---: | :---: | :---: | :---: |
| **M1** Avg / Max Ops | W3 (`N = 100,000`) | `TODO` | `TODO` | `TODO` | `TODO` |
| **M2** Scaling (`ns/op`) | W3 (`N = 1k, 10k, 100k, 1M`) | `TODO` | `TODO` | `TODO` | `TODO` |
| **M3** W3 `replay` (median ms) | W3 (`N = 100,000`) | `TODO` | `TODO` | `TODO` | `TODO` |
| **M3** W3 `events` (median ms) | W3 (`N = 100,000`) | `TODO` | `TODO` | `TODO` | `TODO` |
| **M4** Worst-Case (median ms) | Decreasing `N = 100,000` | `TODO` | `TODO` | `TODO` | `TODO` |
| **M5** Memory (`sizeof` / bytes per elem) | `N = 100,000` | `TODO` | `TODO` | `TODO` | `TODO` |

- **Plots:** Link `results/plots/m2_scaling.png` and `results/plots/m3_comparison.png` once generated.

---

## Part 6: Evaluation & Prediction-vs-Reality Analysis

> **Status: Pending.** Complete after Part 5 measurements are collected.

- **Prediction Accuracy:** Compare Part 4 predicted ratios (~540x ops / 15x–50x time on W3 `replay`; ~2x–10x on W3 `events`; ~1.0x vs. `std::map`; ~3,000x ops on M4) against Part 5 actuals.
- **Explaining Gaps:** Analyze hardware and implementation effects (L1/L2/L3 cache misses on AVL pointer traversal vs. vector prefetching, allocator fragmentation, move-constructor cost of `std::vector` headers during array shifts, and AVL rotation frequency vs. `std::map` RB-tree fixups).
- **Final Verdict:** State when AVL is clearly superior (dynamic workloads with frequent `SUB`/`UNSUB` and large threshold counts) vs. when a Sorted Dynamic Array would be preferable (static or read-only price lists where memory compactness and cache-friendly binary searches dominate).
