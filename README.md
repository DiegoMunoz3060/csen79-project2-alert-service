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
