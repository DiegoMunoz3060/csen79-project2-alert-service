#pragma once

#include <cctype>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace team {

enum class CommandType {
    Ignore,
    Sub,
    Unsub,
    Pub,
    Price,
    Tick,
    Inbox,
    History,
    Stats,
    Error
};

enum class SubMode {
    Immediate,
    At,
    Every,
    Below
};

struct ParsedCommand {
    CommandType type = CommandType::Ignore;
    SubMode sub_mode = SubMode::Immediate;
    std::string user;
    std::string topic;
    std::string message;
    std::string options_text;  // Original SUB option text for HISTORY
    std::string error_msg;
    std::int64_t int_val = 0;  // cents (Below/Price), minutes (Tick), hours (Every), minutes-of-day (At)
    std::size_t limit = 0;     // optional n for INBOX / HISTORY
    bool has_limit = false;
};
// Converts a digit-only string into a 64-bit integer.
inline bool parse_nonneg_int64(const std::string& s, std::int64_t& out) {
    if (s.empty()) {
        return false;
    }
    std::int64_t val = 0;
    constexpr std::int64_t max_v = std::numeric_limits<std::int64_t>::max();
    for (unsigned char ch : s) {
        if (!std::isdigit(ch)) {
            return false;
        }
        int d = ch - '0';
        if (val > (max_v - d) / 10) {
            return false;
        }
        val = val * 10 + d;
    }
    out = val;
    return true;
}

// Parses a non-negative decimal price string into exact integer cents.
// Accepts formats like "600", "50.1" (-> 5010), "579.00" (-> 57900). Never uses double.
inline bool parse_cents(const std::string& s, std::int64_t& out) {
    if (s.empty()) {
        return false;
    }
    std::size_t dot = s.find('.');
    std::string whole_part = (dot == std::string::npos) ? s : s.substr(0, dot);
    std::int64_t dollars = 0;
    if (!parse_nonneg_int64(whole_part, dollars)) {
        return false;
    }
    std::int64_t cents = 0;
    if (dot != std::string::npos) {
        std::string frac = s.substr(dot + 1);
        if (frac.empty() || frac.size() > 2) {
            return false;
        }
        for (unsigned char ch : frac) {
            if (!std::isdigit(ch)) {
                return false;
            }
        }
        if (frac.size() == 1) {
            cents = static_cast<std::int64_t>(frac[0] - '0') * 10;
        } else {
            cents = static_cast<std::int64_t>(frac[0] - '0') * 10 +
                    static_cast<std::int64_t>(frac[1] - '0');
        }
    }
    constexpr std::int64_t max_dollars = (std::numeric_limits<std::int64_t>::max() - 99) / 100;
    if (dollars > max_dollars) {
        return false;
    }
    out = dollars * 100 + cents;
    return true;
}

inline std::string format_cents(std::int64_t cents) {
    bool neg = cents < 0;
    std::uint64_t abs_cents = neg ? (static_cast<std::uint64_t>(-(cents + 1)) + 1ULL)
                                  : static_cast<std::uint64_t>(cents);
    std::uint64_t dollars = abs_cents / 100ULL;
    std::uint64_t rem = abs_cents % 100ULL;
    std::string s;
    if (neg) {
        s.push_back('-');
    }
    s += std::to_string(dollars);
    s.push_back('.');
    if (rem < 10) {
        s.push_back('0');
    }
    s += std::to_string(rem);
    return s;
}

inline std::string format_time(std::int64_t total_minutes) {
    std::int64_t day = total_minutes / 1440;
    std::int64_t rem = total_minutes % 1440;
    std::int64_t hour = rem / 60;
    std::int64_t minute = rem % 60;
    std::string s = "D" + std::to_string(day + 1) + " ";
    if (hour < 10) s.push_back('0');
    s += std::to_string(hour);
    s.push_back(':');
    if (minute < 10) s.push_back('0');
    s += std::to_string(minute);
    return s;
}

inline bool parse_hhmm(const std::string& s, std::int64_t& minutes_of_day) {
    if (s.size() != 5 || s[2] != ':') {
        return false;
    }
    for (std::size_t i : {std::size_t{0}, std::size_t{1}, std::size_t{3}, std::size_t{4}}) {
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) {
            return false;
        }
    }
    int hh = (s[0] - '0') * 10 + (s[1] - '0');
    int mm = (s[3] - '0') * 10 + (s[4] - '0');
    if (hh < 0 || hh > 23 || mm < 0 || mm > 59) {
        return false;
    }
    minutes_of_day = static_cast<std::int64_t>(hh * 60 + mm);
    return true;
}

inline bool is_valid_topic(const std::string& topic) {
    if (topic.empty() || topic.front() == '/' || topic.back() == '/') {
        return false;
    }
    for (std::size_t i = 0; i < topic.size(); ++i) {
        unsigned char ch = static_cast<unsigned char>(topic[i]);
        if (std::isspace(ch)) {
            return false;
        }
        if (topic[i] == '/' && i + 1 < topic.size() && topic[i + 1] == '/') {
            return false;
        }
    }
    return true;
}

inline std::vector<std::string> split_ws(const std::string& s) {
    std::vector<std::string> tokens;
    std::size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
            ++i;
        }
        if (i >= s.size()) {
            break;
        }
        std::size_t start = i;
        while (i < s.size() && !std::isspace(static_cast<unsigned char>(s[i]))) {
            ++i;
        }
        tokens.push_back(s.substr(start, i - start));
    }
    return tokens;
}

inline ParsedCommand make_error(const std::string& msg) {
    ParsedCommand cmd;
    cmd.type = CommandType::Error;
    cmd.error_msg = msg;
    return cmd;
}

inline ParsedCommand parse_command(const std::string& raw_line) {
    std::string line = raw_line;
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }

    std::size_t pos = 0;
    while (pos < line.size() && std::isspace(static_cast<unsigned char>(line[pos]))) {
        ++pos;
    }
    if (pos == line.size() || line[pos] == '#') {
        return ParsedCommand{};  // CommandType::Ignore
    }

    std::size_t verb_end = pos;
    while (verb_end < line.size() && !std::isspace(static_cast<unsigned char>(line[verb_end]))) {
        ++verb_end;
    }
    std::string verb = line.substr(pos, verb_end - pos);

    if (verb == "PUB") {
        std::size_t t_start = verb_end;
        while (t_start < line.size() && std::isspace(static_cast<unsigned char>(line[t_start]))) {
            ++t_start;
        }
        if (t_start == line.size()) {
            return make_error("ERROR bad command");
        }
        std::size_t t_end = t_start;
        while (t_end < line.size() && !std::isspace(static_cast<unsigned char>(line[t_end]))) {
            ++t_end;
        }
        std::string topic = line.substr(t_start, t_end - t_start);
        if (!is_valid_topic(topic)) {
            return make_error("ERROR bad command");
        }
        std::size_t m_start = t_end;
        while (m_start < line.size() && std::isspace(static_cast<unsigned char>(line[m_start]))) {
            ++m_start;
        }
        if (m_start == line.size()) {
            return make_error("ERROR bad command");
        }
        std::size_t m_end = line.size();
        while (m_end > m_start && std::isspace(static_cast<unsigned char>(line[m_end - 1]))) {
            --m_end;
        }
        ParsedCommand cmd;
        cmd.type = CommandType::Pub;
        cmd.topic = std::move(topic);
        cmd.message = line.substr(m_start, m_end - m_start);
        return cmd;
    }

    std::vector<std::string> tokens = split_ws(line);
    if (verb == "SUB") {
        if (tokens.size() != 3 && tokens.size() != 5) {
            return make_error("ERROR bad command");
        }
        if (!is_valid_topic(tokens[2])) {
            return make_error("ERROR bad command");
        }
        ParsedCommand cmd;
        cmd.type = CommandType::Sub;
        cmd.user = tokens[1];
        cmd.topic = tokens[2];
        if (tokens.size() == 3) {
            cmd.sub_mode = SubMode::Immediate;
            return cmd;
        }
        const std::string& opt = tokens[3];
        const std::string& arg = tokens[4];
        if (opt == "at") {
            std::int64_t mod = 0;
            if (!parse_hhmm(arg, mod)) {
                return make_error("ERROR bad command");
            }
            cmd.sub_mode = SubMode::At;
            cmd.int_val = mod;
            cmd.options_text = "at " + arg;
            return cmd;
        }
        if (opt == "every") {
            std::int64_t hours = 0;
            if (!parse_nonneg_int64(arg, hours) || hours <= 0) {
                return make_error("ERROR bad command");
            }
            cmd.sub_mode = SubMode::Every;
            cmd.int_val = hours;
            cmd.options_text = "every " + arg;
            return cmd;
        }
        if (opt == "below") {
            std::int64_t cents = 0;
            if (!parse_cents(arg, cents)) {
                return make_error("ERROR bad command");
            }
            cmd.sub_mode = SubMode::Below;
            cmd.int_val = cents;
            cmd.options_text = "below " + arg;
            return cmd;
        }
        return make_error("ERROR bad command");
    }

    if (verb == "UNSUB") {
        if (tokens.size() != 3 || !is_valid_topic(tokens[2])) {
            return make_error("ERROR bad command");
        }
        ParsedCommand cmd;
        cmd.type = CommandType::Unsub;
        cmd.user = tokens[1];
        cmd.topic = tokens[2];
        return cmd;
    }

    if (verb == "PRICE") {
        if (tokens.size() != 3 || !is_valid_topic(tokens[1])) {
            return make_error("ERROR bad command");
        }
        std::int64_t cents = 0;
        if (!parse_cents(tokens[2], cents)) {
            return make_error("ERROR bad command");
        }
        ParsedCommand cmd;
        cmd.type = CommandType::Price;
        cmd.topic = tokens[1];
        cmd.int_val = cents;
        return cmd;
    }

    if (verb == "TICK") {
        if (tokens.size() != 2) {
            return make_error("ERROR bad command");
        }
        std::int64_t mins = 0;
        if (!parse_nonneg_int64(tokens[1], mins)) {
            return make_error("ERROR bad command");
        }
        ParsedCommand cmd;
        cmd.type = CommandType::Tick;
        cmd.int_val = mins;
        return cmd;
    }

    if (verb == "INBOX" || verb == "HISTORY") {
        if (tokens.size() != 2 && tokens.size() != 3) {
            return make_error("ERROR bad command");
        }
        ParsedCommand cmd;
        cmd.type = (verb == "INBOX") ? CommandType::Inbox : CommandType::History;
        cmd.user = tokens[1];
        if (tokens.size() == 3) {
            std::int64_t n = 0;
            if (!parse_nonneg_int64(tokens[2], n)) {
                return make_error("ERROR bad command");
            }
            cmd.has_limit = true;
            cmd.limit = static_cast<std::size_t>(n);
        }
        return cmd;
    }

    if (verb == "STATS") {
        if (tokens.size() != 1) {
            return make_error("ERROR bad command");
        }
        ParsedCommand cmd;
        cmd.type = CommandType::Stats;
        return cmd;
    }

    return make_error("ERROR unknown command");
}

}  // namespace team
