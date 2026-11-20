// benchmarks/generator.cpp
// Generates deterministic Project 2 workloads (W1, W2, W3, M4).
//
// Usage:
//   generator --workload W1|W2|W3|M4 [--subs 100000] [--events 10000]
//             [--seed 42] [--output path.txt]

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "../tools/rng.h"

namespace {

struct GenConfig {
    //TODO: data types for better memeory management? why long long? 
    std::string workload = "W1"; //interchangable? 
    long long subs = 100000;    // peak? 
    long long events = 10000;   //Fan-out latency per published event with 100k subscribers?
    unsigned long long seed = 42;
    std::string output_path;  // empty = stdout, are we counting the cli printout in our timining harness? 
};

GenConfig parse_args(int argc, char** argv) {
    GenConfig cfg;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto val = [&]() -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "missing value for " << a << '\n';
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--workload") cfg.workload = val();
        else if (a == "--subs") cfg.subs = std::stoll(val());
        else if (a == "--events") cfg.events = std::stoll(val());
        else if (a == "--seed") cfg.seed = std::stoull(val());
        else if (a == "--output") cfg.output_path = val();
        else {
            std::cerr << "unknown option " << a << '\n';
            std::exit(2);
        }
    }
    if (cfg.workload != "W1" && cfg.workload != "W2" &&
        cfg.workload != "W3" && cfg.workload != "M4") {
        std::cerr << "workload must be W1, W2, W3, or M4\n";
        std::exit(2);
    }
    if (cfg.subs < 0 || cfg.events < 0) {
        std::cerr << "--subs and --events must be >= 0\n";
        //TODO look  PUB to a topic with no subscribers, or a PRICE that meets no threshold, changes nothing and prints nothing.
        std::exit(2);
    }
    return cfg;
}

std::string user_name(long long u) {
    return "u" + std::to_string(u);
}

// M4 worst-case workload:
// 1. Strictly ascending price thresholds on items/phones/item0 (degrades unbalanced BSTs to O(N)).
// 2. Crowded topic news/crowd with front-to-back UNSUB (exposes O(N) vector/list scans and shifts).
// 3. Repeated PUB flood to news/flood (overflows inbox capacity K to expose O(K) front-erase shifts).

void generate_m4(const GenConfig& cfg, kit::Rng& rng, std::string& out) {
    const bool has_flood = cfg.subs >= 1;
    const long long remaining = has_flood ? (cfg.subs - 1) : 0;
    const long long price_subs = remaining / 2;
    const long long crowd_subs = remaining - price_subs;

    // Phase 1: All leading SUB commands.
    for (long long i = 0; i < price_subs; ++i) {
        long long threshold = 10001 + i;  // strictly ascending cents
        out += "SUB " + user_name(i) + " items/phones/item0 below " +
               kit::cents(threshold) + "\n";
    }
    for (long long i = 0; i < crowd_subs; ++i) {
        out += "SUB " + user_name(i) + " news/crowd\n";
    }
    if (has_flood) {
        out += "SUB " + user_name(0) + " news/flood\n";
    }

    // Phase 2: Adversarial event stream.
    long long cur_price = 10001 + price_subs;
    long long unsub_idx = 0;

    for (long long e = 0; e < cfg.events; ++e) {
        double r = rng.uniform();
        if (r < 0.40) {
            if (cur_price > 100) --cur_price;
            out += "PRICE items/phones/item0 " + kit::cents(cur_price) + "\n";
        } else if (r < 0.70) {
            out += "PUB news/flood m" + std::to_string(e) + "\n";
        } else if (r < 0.95 && unsub_idx < crowd_subs) {
            out += "UNSUB " + user_name(unsub_idx) + " news/crowd\n";
            ++unsub_idx;
        } else {
            out += "INBOX " + user_name(0) + "\n";
        }
    }
}
//what is this based on? where in the pdf? can we make this up? do the examples include these? 
void generate_standard(const GenConfig& cfg, kit::Rng& rng, std::string& out) {
    const char* news[] = {"world", "sports", "tech", "business", "science",
                          "health", "politics", "culture", "travel", "local"};
    const char* signs[] = {"aries", "taurus", "gemini", "cancer", "leo", "virgo",
                           "libra", "scorpio", "sagittarius", "capricorn", "aquarius", "pisces"};
    const char* kinds[] = {"phones", "laptops", "tvs", "cameras", "headphones"};

    std::vector<std::string> parents, leaves, items;
    parents.push_back("news");
    for (auto c : news) parents.push_back(std::string("news/") + c);
    for (auto c : news) {
        for (int k = 0; k < 50; ++k) {
            leaves.push_back(std::string("news/") + c + "/s" + std::to_string(k));
        }
    }
    for (auto s : signs) {
        leaves.push_back(std::string("horoscope/") + s);
    }
    for (auto k : kinds) {
        for (int j = 0; j < 50; ++j) {
            items.push_back(std::string("items/") + k + "/item" + std::to_string(j));
        }
    }

    kit::Zipf leaf_zipf(leaves.size(), 1.0);
    kit::Zipf item_zipf(items.size(), 1.0);

    //W3 price storm: 100k thresholds on one item, prices falling step by step.
    long long users = (cfg.workload == "W3") ? cfg.subs : (cfg.subs / 5 > 0 ? cfg.subs / 5 : 1);
    if (users < 1) users = 1; //no users? 

    std::set<std::pair<long long, std::string>> taken;
    std::vector<std::pair<long long, std::string>> made; //TODO: keep track of made alerts. this are delivered status
    std::vector<long long> price(items.size(), 0);
    //range based for loop that generats a random number for all prices. 
    for (auto& p : price) p = rng.between(20000, 150000);

    long long first = 0;
    //W2 hot topic: half of all subscribers follow one topic.
    if (cfg.workload == "W2") {
        for (; first < users / 2 && first < cfg.subs; ++first) {
            taken.insert({first, "news/world"});
            made.push_back({first, "news/world"});
            out += "SUB " + user_name(first) + " news/world\n";
        }
    }

    long long attempts = 0;
    for (long long i = first; i < cfg.subs; ++i) {
        if (++attempts > 50 * cfg.subs + 1000) {
            std::cerr << "could not place every subscription; stopped at " << i << '\n';
            break;  //TODO: are we making the errors? why not let the machine be the bottleneck? 
        }
        long long u = rng.between(0, users - 1);
        std::string topic, extra;
        double r = rng.uniform();
        //W3 price storm: 100k thresholds on one item, prices falling step by step.
        if (cfg.workload == "W3" && r < 0.95) {
            topic = items[0];
            extra = " below " + kit::cents(rng.between(10000, 99999));
        } else if (r < 0.15) {
            std::size_t k = item_zipf.sample(rng);
            topic = items[k];
            extra = " below " + kit::cents(price[k] * rng.between(50, 95) / 100);
        } else {
            topic = rng.chance(0.03)
                        ? parents[static_cast<std::size_t>(
                              rng.between(0, static_cast<long long>(parents.size()) - 1))]
                        : leaves[leaf_zipf.sample(rng)];
            double m = rng.uniform();
            if (topic.rfind("horoscope/", 0) == 0 && m < 0.7) {
                extra = " at " + std::string(rng.chance(0.5) ? "08:00" : "07:30");
            } else if (m < 0.05) {
                extra = " every " + std::to_string(rng.between(1, 12));
            }
        }
        if (!taken.insert({u, topic}).second) {
            --i;
            continue;
        }
        made.push_back({u, topic});
        out += "SUB " + user_name(u) + " " + topic + extra + "\n";
    }

    long long w3_price = 100000;
    for (long long e = 0; e < cfg.events; ++e) {
        double r = rng.uniform();
        if (cfg.workload == "W3" && r < 0.8) {
            w3_price -= rng.between(1, 20);
            if (w3_price < 100) w3_price = 100;
            out += "PRICE " + items[0] + " " + kit::cents(w3_price) + "\n";
        } else if (r < 0.70) {
            std::string topic =
                (cfg.workload == "W2" && rng.chance(0.1))
                    ? "news/world/s0"
                    : leaves[static_cast<std::size_t>(
                          rng.between(0, static_cast<long long>(leaves.size()) - 1))];
            out += "PUB " + topic + " m" + std::to_string(e) + "\n";
        } else if (r < 0.85) {
            std::size_t k = item_zipf.sample(rng);
            price[k] = price[k] * rng.between(85, 105) / 100;
            if (price[k] < 100) price[k] = 100;
            out += "PRICE " + items[k] + " " + kit::cents(price[k]) + "\n";
        } else if (r < 0.95) {
            out += "TICK 30\n";
        } else if (r < 0.98 && !made.empty()) {
            std::size_t k = static_cast<std::size_t>(
                rng.between(0, static_cast<long long>(made.size()) - 1));
            out += "UNSUB " + user_name(made[k].first) + " " + made[k].second + "\n";
            made[k] = made.back();
            made.pop_back();
        } else {
            long long u = rng.between(0, users - 1);
            out += (rng.chance(0.5) ? "INBOX " : "HISTORY ") + user_name(u) + "\n";
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    GenConfig cfg = parse_args(argc, argv);
    kit::Rng rng(cfg.seed);

    std::string out;
    out.reserve(static_cast<std::size_t>(cfg.subs + cfg.events) * 40);

    if (cfg.workload == "M4") {
        generate_m4(cfg, rng, out);
    } else {
        generate_standard(cfg, rng, out);
    }

    if (cfg.output_path.empty()) {
        std::fwrite(out.data(), 1, out.size(), stdout);
    } else {
        std::ofstream fout(cfg.output_path, std::ios::binary);
        if (!fout) {
            std::cerr << "cannot write " << cfg.output_path << '\n';
            return 1;
        }
        fout.write(out.data(), static_cast<std::streamsize>(out.size()));
    }

    std::cerr << "generated workload=" << cfg.workload
              << " subs=" << cfg.subs
              << " events=" << cfg.events
              << " seed=" << cfg.seed << '\n';
    return 0;
}