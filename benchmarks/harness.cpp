#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "kit/bench.h"
#include "kit/counters.h"

// TODO: Include your team's shared headers once finalized.
// #include "parser.h"
// #include "alert_engine.h"

namespace kit {

// ============================================================================
// Section 1: CLI Parsing and Pre-Parsing
// ============================================================================

struct HarnessConfig {
    std::string team = "team0";
    std::string workload = "W1";
    std::string impl = "main";
    std::string operation = "replay";  // "replay" (all cmds) or "events" (post-SUB)
    std::string input_path;
    std::string csv_path = "benchmarks/raw_csv/results.csv";
    int runs = 5;
};

// Stub command type. Replace with the parsed command type from parser.h.
struct ParsedCommand {
    std::string raw;
    bool is_sub = false;
};

struct PreparsedInput {
    std::vector<ParsedCommand> commands;
    std::size_t initial_subs = 0;
};

HarnessConfig parse_args(int argc, char** argv) {
    HarnessConfig cfg;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "missing value for " << a << '\n';
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--team") cfg.team = next();
        else if (a == "--workload") cfg.workload = next();
        else if (a == "--impl") cfg.impl = next();
        else if (a == "--op") cfg.operation = next();
        else if (a == "--input") cfg.input_path = next();
        else if (a == "--csv") cfg.csv_path = next();
        else if (a == "--runs") cfg.runs = std::stoi(next());
        else {
            std::cerr << "unknown option: " << a << '\n';
            std::exit(2);
        }
    }
    if (cfg.input_path.empty()) {
        std::cerr << "required: --input <path>\n";
        std::exit(2);
    }
    if (cfg.operation != "replay" && cfg.operation != "events") {
        std::cerr << "--op must be replay or events\n";
        std::exit(2);
    }
    if (cfg.runs < 1) {
        std::cerr << "--runs must be >= 1\n";
        std::exit(2);
    }
    return cfg;
}

PreparsedInput load_and_preparse(const std::string& path) {
    std::vector<std::string> lines = kit::read_lines(path);
    PreparsedInput out;
    out.commands.reserve(lines.size());

    bool in_leading_subs = true;
    for (const std::string& line : lines) {
        std::size_t pos = 0;
        while (pos < line.size() && std::isspace(static_cast<unsigned char>(line[pos]))) {
            ++pos;
        }
        if (pos == line.size() || line[pos] == '#') {
            continue;
        }

        // TODO: Call your App Lead's parser function here instead of storing raw text.
        ParsedCommand cmd;
        cmd.raw = line.substr(pos);
        cmd.is_sub = (cmd.raw.rfind("SUB ", 0) == 0);

        if (in_leading_subs && cmd.is_sub) {
            ++out.initial_subs;
        } else {
            in_leading_subs = false;
        }
        out.commands.push_back(std::move(cmd));
    }
    return out;
}

// ============================================================================
// Section 2: Engine Factory / Adapter
// ============================================================================

// Stub engine. Replace with your team's AlertEngine in quiet mode (--quiet).
class EngineStub {
public:
    explicit EngineStub(const std::string& impl_name) : impl_(impl_name) {}

    void execute(const ParsedCommand& cmd) {
        (void)cmd;
        KIT_COUNT(counters_, visits);
    }

    void reset_counters() { counters_.reset(); }

    kit::Counters get_counters() const {
        kit::Counters c = counters_;
        c.extra["bytes_per_elem"] = bytes_per_element();
        return c;
    }

    std::uint64_t bytes_per_element() const {
        // TODO: Return sizeof(Node) + overhead for M5.
        return 0;
    }

private:
    std::string impl_;
    kit::Counters counters_;
};

}  // namespace
using namespace kit; 

int main(int argc, char** argv) {
    // Section 1: Read and parse input before starting the clock.
    HarnessConfig cfg = parse_args(argc, argv);
    PreparsedInput input = load_and_preparse(cfg.input_path);

    std::size_t timed_begin = 0;
    if (cfg.operation == "events") {
        timed_begin = input.initial_subs;
    }
    const std::size_t timed_end = input.commands.size();
    const std::uint64_t op_count =
        (timed_end > timed_begin) ? static_cast<std::uint64_t>(timed_end - timed_begin) : 0;

    // Section 2 & 3: Fresh setup, steady_clock timed body, median of N runs.
    std::unique_ptr<EngineStub> engine;
    kit::Counters one_run_counters;

    auto setup = [&]() {
        engine = std::make_unique<EngineStub>(cfg.impl);
        if (cfg.operation == "events") {
            for (std::size_t i = 0; i < timed_begin; ++i) {
                engine->execute(input.commands[i]);
            }
            engine->reset_counters();
        }
    };

    auto body = [&]() {
        for (std::size_t i = timed_begin; i < timed_end; ++i) {
            engine->execute(input.commands[i]);
        }
    };

    auto after = [&]() {
        // Deterministic workload: each run produces identical counts for one run.
        one_run_counters = engine->get_counters();
    };

    double median = kit::median_ns(cfg.runs, setup, body, after);

    // Section 4: Write one standardized CSV row.
    kit::CsvWriter csv(cfg.csv_path);
    csv.row({
        cfg.team,
        "alerts",
        cfg.workload,
        cfg.impl,
        op_count,
        cfg.operation,
        cfg.runs,
        median,
        one_run_counters
    });

    double ns_per_op = op_count ? median / static_cast<double>(op_count) : 0.0;
    std::cerr << cfg.workload << " [" << cfg.impl << "] " << cfg.operation
              << " n=" << op_count << " median_ns=" << median
              << " ns_per_op=" << ns_per_op << '\n';
    return 0;
}