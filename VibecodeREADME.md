# csen79-project2-alert-service

Starter scaffold for the CSEN 79 project. The directories and source files are in place; implementation details and the starter-kit output format still need to be filled in.

## Build

```sh
cmake -S . -B build
cmake --build build
```

Or use `make build`. The `test` and `bench` targets are placeholders until their harnesses and workloads are implemented.

## Implementation roadmap

1. Implement the interfaces and candidate data structures in `include/`.
2. Add parsing, delivery rules, and simulated time in `src/`.
3. Add unit and end-to-end tests, then benchmark workloads and record results.
4. Complete the design decision records, sanitizer results, and contribution notes in `docs/`.

## Implementation selection

The planned CLI supports `--impl=main|alt`, `--quiet`, and a configurable inbox size `K`. These flags are not implemented yet.

---

## Project Roadmap & Schedule

> **Target Demo Date:** Week 11  
> **Development Duration:** 1 Week  
> **Team Size:** 4 Members (3 Hand-Built Core Structures + 2 Alternative Comparisons)

| Phase | Target Date | Milestone Goal | Definition of Done (Verification) | Owner(s) |
| :--- | :--- | :--- | :--- | :--- |
| **Phase 1** | `YYYY-MM-DD` | Pick project, split ownership, and write draft DDRs (Parts 1–4). | Every core structure has an assigned owner and a written complexity prediction before measuring. | All Members |
| **Phase 2** | `YYYY-MM-DD` | Implement hand-built structures with unit tests. | Standalone class template headers and passing unit tests are pushed to the team repo. | Structures, App, & Benchmark Leads |
| **Phase 3** | `YYYY-MM-DD` | Wire structures into the application command loop. | The sample command script runs end-to-end and matches `OUTPUT_FORMAT.md`. | App Lead & Quality Lead |
| **Phase 4** | `YYYY-MM-DD` | Add instrumentation counters, workload generator, and `--impl` swap flag. | `STATS` prints real operation counts and Workload 1 (`W1`) runs cleanly at `N = 100,000`. | Structures, App, & Benchmark Leads |
| **Phase 5** | `YYYY-MM-DD` | Execute full benchmark runs (`M1` to `M5`) across all workloads (`W1` to `W3`). | Raw CSV results and `M2`/`M3` plots are generated and committed to the repo. | Benchmark Lead |
| **Phase 6** | `YYYY-MM-DD` | Finish DDRs (Parts 5–6), fix bugs, run sanitizers, and rehearse demo. | Clean sanitizer log is committed, prediction gaps are explained, and the demo runs in under 8 minutes. | Quality Lead & All Members |
| **Phase 7** | **Week 11** | Submit repository and deliver live in-class demo. | Final release is tagged in Git and all members are prepared for live "what-if" questions. | All Members |

---

## Team Roles & Actionable Checklists

> **Grading & Ownership Rule:** Every team member owns one graded deliverable (one hand-built data structure and its DDR, or the benchmark harness / test suite) and must be able to explain any structure in the project during the live Q&A.

### 1. Structures Lead — `@XXDNYUDWUW (Erich L)`

**Primary Role:** Sets the class template interface, defines the instrumentation counter conventions, and reviews every structure.  
**Graded Deliverable:** Core Structure 1 (Hierarchical Topic Tree/Trie) + DDR 1.

#### Design & Architecture
- [ ] Draft **DDR 1 (Parts 1–4)**: define the user requirement, estimate operation frequencies, select two candidate structures, and calculate the predicted Big-O ratio at `N = 100,000`.
- [ ] Design the shared instrumentation header to count `comparisons`, `elements visited`, `allocations`, and `elements moved`.
- [ ] Ensure counters can be printed, reset, and either left on with negligible overhead during `-O2` benchmarks or toggled with a compile flag.
- [ ] Define the shared C++ interface so the STL baseline containers and hand-built replacements share identical method signatures.

#### Implementation & Testing
- [ ] Implement **Core Structure 1** from scratch as a standalone class template in its own header file.
- [ ] Implement the alternative structure or instrumented STL wrapper for **Decision 1** (`--impl=alt`).
- [ ] Write unit tests in `doctest` or `Catch2` covering empty structures, single elements, duplicates, and removing the first, last, and only element.

#### Review & Documentation
- [ ] Code-review Core Structures 2 and 3 to verify structural invariants hold after every operation and counters increment accurately.
- [ ] Calculate **M5 (Memory)** for all structures: bytes per stored element (`sizeof(Node) * count` plus container overhead) for both chosen and alternative designs.
- [ ] Complete **DDR 1 (Parts 5–6)** once `M1`–`M5` benchmark data is collected, explaining any gap between prediction and measurement.

---

### 2. App Lead — `@Krishy2007 (Madhav Korimerla)`

**Primary Role:** Builds the command parser, implements the alert delivery engine, and wires the `--impl=main|alt` swap flag.  
**Graded Deliverable:** Core Structure 2 (Price Threshold / Scheduled Delivery Tree) + DDR 2.

#### Design & Architecture
- [ ] Draft **DDR 2 (Parts 1–4)**: define the requirement, operation frequencies, two candidates, and predicted ratio at `N = 100,000`.
- [ ] Design the state machine for all 8 commands: `SUB`, `UNSUB`, `PUB`, `PRICE`, `TICK`, `INBOX`, `HISTORY`, and `STATS`.

#### Implementation & Testing
- [ ] Implement **Core Structure 2** from scratch as a standalone class template in its own header with full counter instrumentation.
- [ ] Write unit tests covering empty state, single element, duplicates, and removing the first, last, and only element.
- [ ] Build the CLI runner to read commands from a file or `stdin`, configure inbox capacity `K` at startup, silence output on `--quiet` (except `STATS`), and swap structures on `--impl=main|alt`.
- [ ] Enforce all 7 delivery rules in the application loop:
  - [ ] Start the simulated clock at `Day 1, 00:00` and deliver immediate alerts at the current timestamp.
  - [ ] Replace a user's existing subscription if a second `SUB` targets the exact same topic.
  - [ ] Deduplicate alerts when multiple subscriptions match a topic hierarchy (e.g., `news/sports` and `news/sports/soccer`), using the most specific match to govern delivery timing.
  - [ ] Make `UNSUB` remove only the exact topic and cancel its waiting deliveries without scanning all topic subscribers.
  - [ ] Trigger `PRICE` alerts only when the price drops strictly below the threshold, fire once, and remove the subscription without scanning all thresholds on that item.
  - [ ] Queue `at <hh:mm>` and `every <hours>` deliveries to hand over collected events in publish order.
  - [ ] Advance the clock on `TICK <minutes>` and fire due deliveries in chronological order, breaking ties by scheduling order.

#### Review & Documentation
- [ ] Verify standard output matches `OUTPUT_FORMAT.md` exactly using `diff`.
- [ ] Complete **DDR 2 (Parts 5–6)** after benchmark measurements are finalized.

---

### 3. Benchmark Lead — `@Member_3_GitHub`

**Primary Role:** Builds the seeded workload generator, the timing harness, and the `M1`–`M5` performance plots.  
**Graded Deliverable:** Core Structure 3 (Bounded `K`-Inbox / User History Linear Structure) + DDR 3.

#### Design & Architecture
- [ ] Draft **DDR 3 (Parts 1–4)**: define the requirement, operation frequencies, two candidates, and predicted ratio at `N = 100,000`.
- [ ] Implement the alternative structure or STL baseline wrapper for **Decision 2** so two full structure decisions can be swapped and compared.

#### Implementation & Testing
- [ ] Implement **Core Structure 3** from scratch as a standalone class template in its own header with full counter instrumentation.
- [ ] Ensure `INBOX <user> [n]` retains only the last `K` alerts and `HISTORY <user> [n]` scales proportionally to that user's own activity rather than total system events.
- [ ] Write unit tests covering empty state, single element, duplicates, and boundary removals (first, last, and only element).
- [ ] Build the reproducible workload generator using fixed random seeds for all required distributions:
  - [ ] **W1 (Realistic):** Topic popularity follows a Zipf curve.
  - [ ] **W2 (Hot Topic):** 50% of all subscribers follow a single topic.
  - [ ] **W3 (Price Storm):** 100,000 price thresholds on a single item with prices falling step-by-step.
  - [ ] **M4 (Worst Case):** A targeted workload built to hurt a structure and demonstrate how the chosen structure holds up.

#### Benchmarking & Plotting
- [ ] Build the timing harness following all measurement rules: build with `-O2`, parse all input before starting the clock, run with `--quiet`, measure with `std::chrono::steady_clock`, and report the median of 5 runs along with machine specs.
- [ ] Collect and commit raw CSVs for **M1** (avg/max op counts), **M2** (scaling at `N = 1,000`, `10,000`, `100,000`, and `1,000,000`), **M3** (main vs. alt and STL baselines), **M4** (worst case), and **M5** (memory footprint).
- [ ] Generate log-log scaling plots for **M2** and comparison plots for **M3**.
- [ ] Complete **DDR 3 (Parts 5–6)**.

---

### 4. Quality Lead — `@Member_4_GitHub`

**Primary Role:** Owns the unit and integration test suite, sanitizer verification, master `README.md`, and live demo script.  
**Graded Deliverable:** Comprehensive Test Suite, Sanitizer Verification, & Demo Harness.

#### Build System & Continuous Verification
- [ ] Configure `Makefile` or `CMakeLists.txt` so the entire project compiles in one command using `C++17`, `-O2 -Wall -Wextra`, and produces zero warnings.
- [ ] Set up the unit test runner (`doctest` or `Catch2` single-header) to test all 3 hand-built structures in isolation.

#### Testing & Memory Safety
- [ ] Write independent end-to-end command scripts and expected `.out` files for every delivery rule, verifying correctness automatically with `diff`:
  - [ ] Simulated clock initialization and immediate alert timestamps.
  - [ ] Same-topic subscription replacement.
  - [ ] Hierarchical topic matching and most-specific subscription deduplication.
  - [ ] `UNSUB` exact-topic removal and pending delivery cancellation.
  - [ ] Strict `<` price threshold firing and single-shot subscription removal.
  - [ ] Scheduled `at <hh:mm>` and `every <hours>` batch delivery ordering.
  - [ ] `TICK` tie-breaking by scheduling order and silent no-op handling.
- [ ] Write invariant verification tests for all 3 hand-built structures to confirm links, ordering, and counters remain valid after interleaved insertions and removals.
- [ ] Run the full test suite and benchmark workloads under **AddressSanitizer** or **Valgrind** and commit the clean, zero-leak output log to the repository.

#### Documentation & Live Demo Preparation
- [ ] Write `README.md` documenting the one-command build, CLI usage, `--impl=main|alt` swap flag, and exact benchmark seeds.
- [ ] Collect the one-paragraph individual contribution statements from all 4 team members.
- [ ] Author and rehearse the live **Demo Script** (`demo_script.in`) so the entire presentation—app replay, M3/M4 evidence review, and live `--impl=alt` swap—completes smoothly within 8 minutes.
