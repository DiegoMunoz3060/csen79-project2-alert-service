# Alert Service Data Structures to Implement

CSEN 79 Project 2 implementation plan. The three core structures are a hash-backed topic trie, a binary min-heap, and an AVL tree of price thresholds.

## Files and shared records

| File | Responsibility |
| --- | --- |
| `include/team/topic_trie.h` | Slash-delimited topic paths and prefix subscriptions |
| `include/team/delivery_heap.h` | Scheduled delivery deadlines |
| `include/team/price_threshold_avl.h` | Ordered price thresholds for each exact item |
| `include/team/alert_service.h` | Command processing, subscription state, inbox, and history |
| `src/main.cpp` | Input parsing and output formatting |
| `tests/` | Structure tests and command-level tests |

Use C++17. Each hand-built structure exposes `stats()` and `reset_stats()` through the starter kit's `kit::Counters`. Implement hash probing, heap sifting, and AVL balancing directly. Use a vector-style buffer for indexed storage. Use `std::vector` where permitted by the course rules, or a hand-built growable buffer if required.

| Record | Fields |
| --- | --- |
| User | Name, active subscription lookup, inbox, history |
| Subscription | Unique ID, user ID, topic node ID, mode, active flag, optional interval or daily time, optional threshold, pending publications |
| Publication | Sequence number, published topic, message |
| Delivery | Actual delivery minute, user ID, original event sequence, topic, message |
| Scheduled entry | Due minute and subscription ID |
| Item state | Previous price if known and that item's threshold tree |

Store prices as integer cents and time as integer simulation minutes. Store subscription records centrally and keep IDs in indexes. Assign a fresh ID on resubscription. Keep IDs stable across growth and avoid retaining pointers or references into a buffer across a potentially reallocating append. Store pending messages by value or use stable publication IDs with a defined lifetime.

## 1. Topic trie

Use a growable array of nodes. Each node has a child hash table that maps a complete path segment to a node index, and a collection of subscription IDs registered directly on that topic. For example, `news/sports/soccer` takes three transitions. Price subscriptions belong in the price index and must be excluded from ordinary publication matches.

| Suggested operation | Behavior |
| --- | --- |
| `findTopic(path)` | Walk existing edges and return the exact node ID if present |
| `ensureTopic(path)` | Create missing nodes and return the final node ID |
| `addSubscription(node, id)` | Append the ID and record its slot if using indexed deletion |
| `removeSubscription(node, id)` | Swap with the last ID, pop, and update the moved ID's slot |
| `visitMatches(path, visitor)` | Enumerate direct subscribers at every existing prefix node |

Implement collision handling with open addressing and full-segment equality. Allocate a child table on first insertion and grow before its load factor becomes excessive. Retain empty topic nodes initially, so child edges never need tombstones. Count hash probes, key comparisons, table resizes, visited trie nodes, and visited subscribers.

On `PUB`, preserve matches from existing ancestors even if a later segment is missing. Collect one winning subscription per user. Traverse from deepest matching node to shallowest and accept the first match per user, or traverse shallow to deep and overwrite the winner. Apply each winner's delivery mode after deduplication. Bob following both `news` and `news/sports/soccer` receives the soccer story once through the latter. A publication on `news/sportscar` can match `news` and cannot match `news/sports`. Expected lookup work is linear in the path text plus the number of candidate subscriptions inspected under reasonable hashing.

## 2. Binary min-heap

Use an indexed array ordered by `(dueMinute, subscriptionId)`. The ID is a deterministic tie-breaker. For index `i`, children are `2*i+1` and `2*i+2`, and the parent of a non-root entry is `(i-1)/2`.

| Suggested operation | Behavior |
| --- | --- |
| `push(entry)` | Append and sift up while the entry precedes its parent |
| `peek()` | Inspect the earliest entry at the root |
| `pop()` | Replace the root with the last entry and sift down through the smaller child |
| `empty()` | Check whether any entries remain |

Maintain at most one outstanding wakeup per active scheduled subscription. A publication appends to the selected subscription's pending queue. On a wakeup, flush all pending publications in sequence order and compute the next recurrence from the due time or schedule anchor. The next due time must be strictly later than the current one. For `at`, recur daily. For `every`, convert the interval to minutes and preserve the recurrence anchor. Check first-fire behavior for a subscription or publication created exactly on a boundary against the handout.

On unsubscribe, discard pending items and mark the subscription inactive. When popped, skip stale entries. With a fresh ID on resubscribe, an old wakeup cannot deliver for the new subscription. A basic implementation maintains wakeups even for empty queues. Queueing wakeups only while pending work exists is a performance option if it preserves the same delivery times.

On `TICK`, calculate the target, process every heap item with due time at or before the target, timestamp deliveries at their due times, then set the clock to the target and print `TIME`. Pop all due items for a delivery time before formatting that group. Heap push/pop are O(log h) for h queued entries. Count comparisons, swaps, pushes, pops, and stale pops.

## 3. AVL tree for price thresholds

Maintain one tree per exact item topic. A node contains a threshold in cents, a bucket of subscriber IDs for that threshold, left and right children, and height. Duplicate keys append to a bucket. Removing the last subscriber removes the node.

| Suggested operation | Behavior |
| --- | --- |
| `insert(threshold, id)` | Find or create the threshold bucket, then rebalance |
| `erase(threshold, id)` | Remove the ID, delete an empty node, and rebalance |
| `visitCrossed(newPrice, oldPrice, visitor)` | Visit every bucket satisfying `newPrice < threshold && threshold <= oldPrice` |

- Maintain the BST ordering and an AVL balance factor from -1 to 1. Update heights bottom-up and implement LL, RR, LR, and RL cases.
- When deleting a node with two children, move the replacement key and its entire bucket. Keep any tracked bucket references or positions valid across node replacement.
- During a range query, descend left only if the current key exceeds the lower bound, and right only if the current key is below the upper bound.
- For a known price, a flat or rising price yields no downward crossings. Save the latest price so a later fall can trigger again. Check the handout for the first-ever price and for subscriptions added while a price is already below their threshold.
- With m distinct thresholds, k crossed keys, and a returned subscriptions, the query costs O(log m + k + a). Insertion and node deletion cost O(log m) plus bucket operations. Count key comparisons, visits, rotations, and returned subscribers.

For the sample, 699.00 to 579.00 crosses 600.00 and alerts Erin and Gina. The next move to 549.00 crosses 550.00 and alerts Frank. Landing exactly on 600.00 does not satisfy `below 600.00`.

## Command flow

| Command | Work |
| --- | --- |
| `SUB` | Validate topic and options and check active membership by user and exact topic. Create the record, register it with the applicable structures, and append a history entry preserving options as typed. |
| `UNSUB` | Find active membership, remove it from the relevant lookup index, invalidate queued work, and append history. |
| `PUB` | Find topic prefixes, choose the deepest match per user, then deliver immediately or append to the winner's queue. |
| `PRICE` | Look up the exact item, calculate downward crossings using its last price, deliver to crossed subscriptions, and update the price. |
| `TICK` | Process scheduled deliveries through the target minute, then emit the final time. |
| `INBOX` | Read delivered alerts newest first and apply the optional limit. |
| `HISTORY` | Read subscription changes and deliveries oldest first. Apply the optional limit according to the governing spec. |
| `STATS` | Print operation counts from the three structures. |

Validate commands fully before mutating state. Maintain a user lookup by name and a lookup for the user's active subscriptions by exact topic. A small per-user subscription array is a starting option if its scan cost is acceptable. Centralize delivery recording so immediate, scheduled, and price events update both inbox and history. Use a monotonic sequence number for publication and price events. Where required by the output spec, sort a delivery-time group by username and then original sequence. Preserve delivery state in `--quiet` and skip display-only formatting and sorting.

## Output contract to verify

The repository's output guide and checked-in golden sample disagree. The guide specifies D1, command-numbered errors, price messages of the form `price <new price>`, and newest-first inbox output. The sample starts with D0, uses `OK` and `END`, and shows Bob's inbox in publish order. Confirm the current instructor specification before locking the formatter and golden tests. The sample commands still exercise the core matching, recurrence, and price-crossing logic.

- [Output guide](https://github.com/DiegoMunoz3060/csen79-project2-alert-service/blob/main/docs/OUTPUT_FORMAT.md)
- [Sample input](https://github.com/DiegoMunoz3060/csen79-project2-alert-service/blob/main/samples/p2_demo.txt)
- [Sample expected output](https://github.com/DiegoMunoz3060/csen79-project2-alert-service/blob/main/samples/p2_demo.expected.txt)

## Implementation and verification checklist

1. Implement the trie and test exact matches, ancestor matches, segment boundaries, collisions, resizing, and removal from a crowded node.
2. Implement the heap and test empty state, equal deadlines, unsorted insertion, multiple due times in one tick, exact tick boundaries, and stale entries after unsubscribe and resubscribe.
3. Implement the AVL tree and test all four rotation cases, root deletion, two-child deletion, multiple subscribers at one threshold, inclusive upper and exclusive lower bounds, and falling prices after a rise. Check ordering, heights, and balance after every mutation in structure tests.
4. Integrate the service. Test overlapping subscriptions with different delivery modes, multiple queued publications, same-time output ordering, history and inbox limits, invalid commands with no state mutation, and quiet mode with equivalent delivery state.
5. Build and run doctest, compare command output with a fixture based on the confirmed contract, and run both the tests and the app under AddressSanitizer and UBSan.
6. Run W1, W2, and W3 with fixed seeds. W1 exercises mixed topics, W2 stresses a hot topic, and W3 stresses threshold lookup. Record event and subscription counts, seed, build flags, replay timing, and counters. Compare candidates inspected against actual deliveries.

See the [starter README](https://github.com/DiegoMunoz3060/csen79-project2-alert-service/blob/main/README.md) for build flags and timing support and the [generator](https://github.com/DiegoMunoz3060/csen79-project2-alert-service/blob/main/tools/gen_alerts.cpp) for workload options.
