# Contributions

## Madhav Korimerla (`@Krishy2007`) — App Lead & Core Structure 2
Designed and implemented the Alert Service application layer, including the strict command parser (`include/team/commands.h`, parsing prices in exact integer cents without `double`), the templated `AlertService` engine (`include/team/alert_service.h`) enforcing hierarchical topic deduplication, scheduled deliveries, price crossings, bounded inbox/history tracking, and `--quiet`/`STATS` handling, and the CLI entry point (`src/main.cpp`) supporting `--impl=main|alt|stl` and `--inbox K`. Authored Design Decision Record 2 (`docs/ddr_structure_2.md`) and implemented Core Structure 2 from scratch: the hand-built `PriceThresholdAVL` tree (`include/team/price_threshold_avl.h`), its `SortedThresholdArray` alternative (`include/team/sorted_threshold_array.h`), and the instrumented `StlPriceThresholdMap` baseline (`include/team/stl_price_threshold.h`), along with the `TrieTopicStore` integration adapter (`include/team/trie_topic_store.h`) and comprehensive `doctest` unit, invariant, and multi-implementation parity suites (`tests/test_structure_2.cpp`, `tests/test_app.cpp`).

## Erich L (`@XXDNYUDWUW`) — Structures Lead & Core Structure 1
TODO: Add one paragraph describing contributions.

## Diego Munoz Arroyo (`@DiegoMunoz3060`) — Benchmark Lead & Core Structure 3
TODO: Add one paragraph describing contributions.

## Quality Lead
TODO: Add one paragraph describing contributions.
