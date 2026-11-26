#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include "team/alert_service.h"

namespace {

struct CliOptions {
    std::string impl = "main";
    bool quiet = false;
    std::size_t inbox_k = 10;
    std::string input_file;
};

bool parse_size(const std::string& s, std::size_t& out) {
    std::int64_t v = 0;
    if (!team::parse_nonneg_int64(s, v)) {
        return false;
    }
    out = static_cast<std::size_t>(v);
    return true;
}

CliOptions parse_cli(int argc, char** argv) {
    CliOptions opts;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto need_val = [&](const std::string& flag) -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "missing value for " << flag << '\n';
                std::exit(2);
            }
            return argv[++i];
        };

        if (a == "--quiet") {
            opts.quiet = true;
        } else if (a == "--impl") {
            opts.impl = need_val(a);
        } else if (a.rfind("--impl=", 0) == 0) {
            opts.impl = a.substr(7);
        } else if (a == "--inbox") {
            if (!parse_size(need_val(a), opts.inbox_k)) {
                std::cerr << "invalid value for --inbox\n";
                std::exit(2);
            }
        } else if (a.rfind("--inbox=", 0) == 0) {
            if (!parse_size(a.substr(8), opts.inbox_k)) {
                std::cerr << "invalid value for --inbox\n";
                std::exit(2);
            }
        } else if (!a.empty() && a[0] != '-' && opts.input_file.empty()) {
            opts.input_file = a;
        } else {
            std::cerr << "unknown option: " << a << '\n';
            std::exit(2);
        }
    }
    if (opts.impl != "main" && opts.impl != "alt" && opts.impl != "stl") {
        std::cerr << "--impl must be main, alt, or stl\n";
        std::exit(2);
    }
    return opts;
}

}  // namespace

int main(int argc, char** argv) {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    CliOptions opts = parse_cli(argc, argv);

    std::ifstream file_in;
    std::istream* in = &std::cin;
    if (!opts.input_file.empty()) {
        file_in.open(opts.input_file);
        if (!file_in) {
            std::cerr << "cannot open input file: " << opts.input_file << '\n';
            return 2;
        }
        in = &file_in;
    }

    if (opts.impl == "main") {
        team::MainAlertService service(opts.inbox_k, opts.quiet);
        service.run_stream(*in, std::cout);
    } else if (opts.impl == "alt") {
        team::AltAlertService service(opts.inbox_k, opts.quiet);
        service.run_stream(*in, std::cout);
    } else {
        team::StlAlertService service(opts.inbox_k, opts.quiet);
        service.run_stream(*in, std::cout);
    }
    return 0;
}
