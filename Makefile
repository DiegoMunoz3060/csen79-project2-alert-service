BUILD_DIR ?= build

.PHONY: build test bench clean

build:
	cmake -S . -B $(BUILD_DIR)
	cmake --build $(BUILD_DIR)

test: build
	@echo "Test harness scaffolded; add tests and wire them into CMake."

bench: build
	@echo "Benchmark scaffolded; implement workloads before running benchmarks."

clean:
	rm -rf $(BUILD_DIR)
