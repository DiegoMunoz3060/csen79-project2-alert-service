#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <istream>
#include <limits>
#include <ostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "kit/counters.h"
#include "team/commands.h"
#include "team/price_threshold_avl.h"
#include "team/sorted_threshold_array.h"
#include "team/stl_inbox.h"
#include "team/stl_price_threshold.h"
#include "team/stl_scheduler.h"
#include "team/stl_topic_store.h"
#include "team/trie_topic_store.h"

namespace team {

struct AlertRecord {
    std::int64_t time = 0;
    std::string topic;
    std::string message;
    bool is_price = false;
    std::int64_t price_new = 0;
    std::int64_t price_threshold = 0;

    std::string formatted_message() const {
        if (is_price) {
            return "price " + format_cents(price_new) + " below " + format_cents(price_threshold);
        }
        return message;
    }
};

enum class HistoryKind {
    Sub,
    Unsub,
    Alert
};

struct HistoryEntry {
    std::int64_t time = 0;
    HistoryKind kind = HistoryKind::Alert;
    std::string topic;
    std::string detail;  // options_text for Sub, message for Pub Alert
    bool is_price = false;
    std::int64_t price_new = 0;
    std::int64_t price_threshold = 0;
};

struct PendingPub {
    std::string topic;
    std::string message;
};

struct SubscriptionRecord {
    std::uint64_t id = 0;
    std::size_t user_id = 0;
    std::string topic;
    SubMode mode = SubMode::Immediate;
    bool active = false;
    std::int64_t period_minutes = 0;  // 1440 for At, hours*60 for Every
    std::int64_t threshold = 0;       // cents for Below
    std::vector<PendingPub> pending;
};

// Parameterized Alert Service engine.
// Swap TopicStore, PriceIndex, Scheduler, or InboxBuffer via template arguments
// to support --impl=main|alt|stl with zero duplicated business logic.
template <
    class TopicStore = StlTopicStore<std::uint64_t>,
    class PriceIndex = StlPriceThresholdMap<std::int64_t, std::uint64_t>,
    class Scheduler = StlDeliveryScheduler,
    class InboxBuffer = StlBoundedInbox<AlertRecord>>
class AlertService {
public:
    explicit AlertService(std::size_t inbox_capacity = 10, bool quiet = false)
        : inbox_capacity_(inbox_capacity), quiet_(quiet) {
        // Reserve slot 0 so valid subscription IDs start at 1.
        subs_.emplace_back();
    }

    void set_quiet(bool quiet) { quiet_ = quiet; }
    bool quiet() const { return quiet_; }
    std::int64_t now_minutes() const { return now_minutes_; }

    void execute_line(const std::string& line, std::ostream& out) {
        ParsedCommand cmd = parse_command(line);
        execute(cmd, out);
    }

    void run_stream(std::istream& in, std::ostream& out) {
        std::string line;
        while (std::getline(in, line)) {
            execute_line(line, out);
        }
    }

    void execute(const ParsedCommand& cmd, std::ostream& out) {
        switch (cmd.type) {
            case CommandType::Ignore:
                return;
            case CommandType::Error:
                if (!quiet_) {
                    out << cmd.error_msg << '\n';
                }
                return;
            case CommandType::Sub:
                handle_sub(cmd, out);
                return;
            case CommandType::Unsub:
                handle_unsub(cmd, out);
                return;
            case CommandType::Pub:
                handle_pub(cmd, out);
                return;
            case CommandType::Price:
                handle_price(cmd, out);
                return;
            case CommandType::Tick:
                handle_tick(cmd, out);
                return;
            case CommandType::Inbox:
                handle_inbox(cmd, out);
                return;
            case CommandType::History:
                handle_history(cmd, out);
                return;
            case CommandType::Stats:
                handle_stats(out);
                return;
        }
    }

    kit::Counters topic_stats() const { return topics_.stats(); }

    kit::Counters price_stats() const {
        kit::Counters total;
        for (const auto& [item, state] : items_) {
            (void)item;
            total += state.thresholds.stats();
        }
        return total;
    }

    kit::Counters scheduler_stats() const { return scheduler_.stats(); }

    kit::Counters inbox_stats() const {
        kit::Counters total;
        for (const auto& u : users_) {
            total += u.inbox.stats();
        }
        return total;
    }

    kit::Counters total_stats() const {
        kit::Counters total = topics_.stats();
        total += price_stats();
        total += scheduler_.stats();
        total += inbox_stats();
        return total;
    }

    void reset_stats() {
        topics_.reset_stats();
        for (auto& [item, state] : items_) {
            (void)item;
            state.thresholds.reset_stats();
        }
        scheduler_.reset_stats();
        for (auto& u : users_) {
            u.inbox.reset_stats();
        }
    }

private:
    struct UserRecord {
        std::string name;
        std::unordered_map<std::string, std::uint64_t> active_subs;
        InboxBuffer inbox;
        std::vector<HistoryEntry> history;

        UserRecord(std::string n, std::size_t inbox_cap)
            : name(std::move(n)), inbox(inbox_cap) {}
    };

    struct ItemPriceState {
        bool has_price = false;
        std::int64_t last_price = 0;
        PriceIndex thresholds;
    };

    std::size_t get_or_create_user(const std::string& name) {
        auto it = user_ids_.find(name);
        if (it != user_ids_.end()) {
            return it->second;
        }
        std::size_t id = users_.size();
        user_ids_.emplace(name, id);
        users_.emplace_back(name, inbox_capacity_);
        pub_seen_epoch_.push_back(0);
        pub_winner_depth_.push_back(0);
        pub_winner_sub_.push_back(0);
        return id;
    }

    void record_pub_delivery(std::int64_t delivery_time,
                             std::size_t user_id,
                             const std::string& topic,
                             const std::string& message,
                             std::ostream& out) {
        auto& u = users_[user_id];
        AlertRecord alert;
        alert.time = delivery_time;
        alert.topic = topic;
        alert.message = message;
        u.inbox.push(alert);

        HistoryEntry hist;
        hist.time = delivery_time;
        hist.kind = HistoryKind::Alert;
        hist.topic = topic;
        hist.detail = message;
        u.history.push_back(std::move(hist));

        if (!quiet_) {
            out << "ALERT " << format_time(delivery_time) << ' ' << u.name << ' '
                << topic << ' ' << message << '\n';
        }
    }

    void record_price_delivery(std::int64_t delivery_time,
                               std::size_t user_id,
                               const std::string& item,
                               std::int64_t new_price,
                               std::int64_t threshold,
                               std::ostream& out) {
        auto& u = users_[user_id];
        AlertRecord alert;
        alert.time = delivery_time;
        alert.topic = item;
        alert.is_price = true;
        alert.price_new = new_price;
        alert.price_threshold = threshold;
        u.inbox.push(alert);

        HistoryEntry hist;
        hist.time = delivery_time;
        hist.kind = HistoryKind::Alert;
        hist.topic = item;
        hist.is_price = true;
        hist.price_new = new_price;
        hist.price_threshold = threshold;
        u.history.push_back(std::move(hist));

        if (!quiet_) {
            out << "ALERT " << format_time(delivery_time) << ' ' << u.name << ' '
                << item << " price " << format_cents(new_price)
                << " below " << format_cents(threshold) << '\n';
        }
    }

    void handle_sub(const ParsedCommand& cmd, std::ostream& out) {
        std::size_t uid = get_or_create_user(cmd.user);
        auto& u = users_[uid];
        if (u.active_subs.find(cmd.topic) != u.active_subs.end()) {
            if (!quiet_) {
                out << "ERROR already subscribed " << cmd.user << ' ' << cmd.topic << '\n';
            }
            return;
        }

        std::uint64_t sub_id = static_cast<std::uint64_t>(subs_.size());
        SubscriptionRecord rec;
        rec.id = sub_id;
        rec.user_id = uid;
        rec.topic = cmd.topic;
        rec.mode = cmd.sub_mode;
        rec.active = true;

        if (cmd.sub_mode == SubMode::Below) {
            rec.threshold = cmd.int_val;
            items_[cmd.topic].thresholds.insert(cmd.int_val, sub_id);
        } else {
            topics_.add_subscription(cmd.topic, sub_id);
            if (cmd.sub_mode == SubMode::At) {
                rec.period_minutes = 1440;
                std::int64_t cur_day = now_minutes_ / 1440;
                std::int64_t cur_tod = now_minutes_ % 1440;
                std::int64_t due = (cmd.int_val > cur_tod)
                                       ? (cur_day * 1440 + cmd.int_val)
                                       : ((cur_day + 1) * 1440 + cmd.int_val);
                scheduler_.push(ScheduledEntry{due, sub_id});
            } else if (cmd.sub_mode == SubMode::Every) {
                rec.period_minutes = cmd.int_val * 60;
                std::int64_t due = now_minutes_ + rec.period_minutes;
                scheduler_.push(ScheduledEntry{due, sub_id});
            }
        }

        subs_.push_back(std::move(rec));
        u.active_subs.emplace(cmd.topic, sub_id);

        HistoryEntry hist;
        hist.time = now_minutes_;
        hist.kind = HistoryKind::Sub;
        hist.topic = cmd.topic;
        hist.detail = cmd.options_text;
        u.history.push_back(std::move(hist));

        if (!quiet_) {
            out << "OK SUB " << cmd.user << ' ' << cmd.topic << '\n';
        }
    }

    void handle_unsub(const ParsedCommand& cmd, std::ostream& out) {
        auto uit = user_ids_.find(cmd.user);
        if (uit == user_ids_.end()) {
            if (!quiet_) {
                out << "ERROR not subscribed " << cmd.user << ' ' << cmd.topic << '\n';
            }
            return;
        }
        auto& u = users_[uit->second];
        auto sit = u.active_subs.find(cmd.topic);
        if (sit == u.active_subs.end()) {
            if (!quiet_) {
                out << "ERROR not subscribed " << cmd.user << ' ' << cmd.topic << '\n';
            }
            return;
        }

        std::uint64_t sub_id = sit->second;
        u.active_subs.erase(sit);

        auto& rec = subs_[sub_id];
        rec.active = false;
        rec.pending.clear();
        if (rec.mode == SubMode::Below) {
            auto iit = items_.find(rec.topic);
            if (iit != items_.end()) {
                iit->second.thresholds.erase(rec.threshold, sub_id);
            }
        } else {
            topics_.remove_subscription(rec.topic, sub_id);
        }

        HistoryEntry hist;
        hist.time = now_minutes_;
        hist.kind = HistoryKind::Unsub;
        hist.topic = cmd.topic;
        u.history.push_back(std::move(hist));

        if (!quiet_) {
            out << "OK UNSUB " << cmd.user << ' ' << cmd.topic << '\n';
        }
    }

    void handle_pub(const ParsedCommand& cmd, std::ostream& out) {
        ++pub_epoch_;
        pub_touched_users_.clear();
        immediate_winners_.clear();

        topics_.visit_matches(cmd.topic, [&](std::size_t depth, std::uint64_t sub_id) {
            const auto& rec = subs_[sub_id];
            if (!rec.active) {
                return;
            }
            std::size_t uid = rec.user_id;
            if (pub_seen_epoch_[uid] != pub_epoch_) {
                pub_seen_epoch_[uid] = pub_epoch_;
                pub_winner_depth_[uid] = depth;
                pub_winner_sub_[uid] = sub_id;
                pub_touched_users_.push_back(uid);
            } else if (depth > pub_winner_depth_[uid]) {
                pub_winner_depth_[uid] = depth;
                pub_winner_sub_[uid] = sub_id;
            }
        });

        for (std::size_t uid : pub_touched_users_) {
            std::uint64_t sub_id = pub_winner_sub_[uid];
            auto& rec = subs_[sub_id];
            if (rec.mode == SubMode::Immediate) {
                immediate_winners_.push_back(sub_id);
            } else {
                rec.pending.push_back(PendingPub{cmd.topic, cmd.message});
            }
        }

        // Order immediate alerts by when the deciding subscription was made (oldest first).
        if (immediate_winners_.size() > 1) {
            std::sort(immediate_winners_.begin(), immediate_winners_.end());
        }
        for (std::uint64_t sub_id : immediate_winners_) {
            const auto& rec = subs_[sub_id];
            record_pub_delivery(now_minutes_, rec.user_id, cmd.topic, cmd.message, out);
        }
    }

    void handle_price(const ParsedCommand& cmd, std::ostream& out) {
        auto& state = items_[cmd.topic];
        std::int64_t old_price = state.has_price
                                     ? state.last_price
                                     : std::numeric_limits<std::int64_t>::max();
        std::int64_t new_price = cmd.int_val;

        if (new_price < old_price && !state.thresholds.empty()) {
            state.thresholds.visit_crossed(
                new_price, old_price, [&](std::int64_t threshold, std::uint64_t sub_id) {
                    const auto& rec = subs_[sub_id];
                    if (!rec.active) {
                        return;
                    }
                    record_price_delivery(now_minutes_, rec.user_id, cmd.topic,
                                          new_price, threshold, out);
                });
        }

        state.has_price = true;
        state.last_price = new_price;
    }

    void handle_tick(const ParsedCommand& cmd, std::ostream& out) {
        std::int64_t target = now_minutes_ + cmd.int_val;

        while (!scheduler_.empty() && scheduler_.peek().due_minute <= target) {
            ScheduledEntry entry = scheduler_.pop();
            auto& rec = subs_[entry.sub_id];
            if (!rec.active) {
                continue;  // Skip stale entry after UNSUB.
            }
            if (!rec.pending.empty()) {
                for (const auto& pub : rec.pending) {
                    record_pub_delivery(entry.due_minute, rec.user_id, pub.topic, pub.message, out);
                }
                rec.pending.clear();
            }
            // Advance straight to the earliest recurrence strictly after `target`
            // (intermediate empty wakeups emit nothing).
            std::int64_t delta = target - entry.due_minute;
            std::int64_t jumps = (delta / rec.period_minutes) + 1;
            std::int64_t next_due = entry.due_minute + jumps * rec.period_minutes;
            scheduler_.push(ScheduledEntry{next_due, entry.sub_id});
        }

        now_minutes_ = target;
        if (!quiet_) {
            out << "TIME " << format_time(now_minutes_) << '\n';
        }
    }

    void handle_inbox(const ParsedCommand& cmd, std::ostream& out) {
        if (quiet_) {
            return;
        }
        auto uit = user_ids_.find(cmd.user);
        if (uit != user_ids_.end()) {
            const auto& u = users_[uit->second];
            std::size_t limit = cmd.has_limit ? cmd.limit : inbox_capacity_;
            u.inbox.visit_last_oldest_first(limit, [&](const AlertRecord& a) {
                out << "ALERT " << format_time(a.time) << ' ' << u.name << ' '
                    << a.topic << ' ' << a.formatted_message() << '\n';
            });
        }
        out << "END\n";
    }

    void handle_history(const ParsedCommand& cmd, std::ostream& out) {
        if (quiet_) {
            return;
        }
        auto uit = user_ids_.find(cmd.user);
        if (uit != user_ids_.end()) {
            const auto& u = users_[uit->second];
            std::size_t total = u.history.size();
            std::size_t start = 0;
            if (cmd.has_limit && cmd.limit < total) {
                start = total - cmd.limit;
            }
            for (std::size_t i = start; i < total; ++i) {
                const auto& h = u.history[i];
                out << format_time(h.time) << ' ';
                if (h.kind == HistoryKind::Sub) {
                    out << "SUB " << h.topic;
                    if (!h.detail.empty()) {
                        out << ' ' << h.detail;
                    }
                    out << '\n';
                } else if (h.kind == HistoryKind::Unsub) {
                    out << "UNSUB " << h.topic << '\n';
                } else {
                    out << "ALERT " << h.topic << ' ';
                    if (h.is_price) {
                        out << "price " << format_cents(h.price_new)
                            << " below " << format_cents(h.price_threshold);
                    } else {
                        out << h.detail;
                    }
                    out << '\n';
                }
            }
        }
        out << "END\n";
    }

    void handle_stats(std::ostream& out) const {
        topic_stats().print(out, "STATS topics");
        price_stats().print(out, "STATS prices");
        scheduler_stats().print(out, "STATS scheduler");
        inbox_stats().print(out, "STATS inbox");
    }

    std::size_t inbox_capacity_ = 10;
    bool quiet_ = false;
    std::int64_t now_minutes_ = 0;

    TopicStore topics_;
    std::unordered_map<std::string, ItemPriceState> items_;
    Scheduler scheduler_;

    std::unordered_map<std::string, std::size_t> user_ids_;
    std::vector<UserRecord> users_;
    std::vector<SubscriptionRecord> subs_;

    // Reusable per-PUB deduplication buffers (zero allocations per PUB).
    std::uint64_t pub_epoch_ = 0;
    std::vector<std::uint64_t> pub_seen_epoch_;
    std::vector<std::size_t> pub_winner_depth_;
    std::vector<std::uint64_t> pub_winner_sub_;
    std::vector<std::size_t> pub_touched_users_;
    std::vector<std::uint64_t> immediate_winners_;
};

using MainAlertService = AlertService<
    TrieTopicStore<std::uint64_t>,
    PriceThresholdAVL<std::int64_t, std::uint64_t>,
    StlDeliveryScheduler,
    StlBoundedInbox<AlertRecord>>;

using AltAlertService = AlertService<
    StlTopicStore<std::uint64_t>,
    SortedThresholdArray<std::int64_t, std::uint64_t>,
    StlDeliveryScheduler,
    StlBoundedInbox<AlertRecord>>;

using StlAlertService = AlertService<
    StlTopicStore<std::uint64_t>,
    StlPriceThresholdMap<std::int64_t, std::uint64_t>,
    StlDeliveryScheduler,
    StlBoundedInbox<AlertRecord>>;

}  // namespace team
