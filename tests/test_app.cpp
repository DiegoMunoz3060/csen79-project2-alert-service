#include <fstream>
#include <sstream>
#include <string>

#include "doctest/doctest.h"
#include "team/alert_service.h"

namespace {

std::string strip_stats(const std::string& text) {
    std::istringstream in(text);
    std::ostringstream out;
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("STATS", 0) == 0) {
            continue;
        }
        out << line << '\n';
    }
    return out.str();
}

std::string run_script(const std::string& script, std::size_t inbox_k = 10, bool quiet = false) {
    team::StlAlertService svc(inbox_k, quiet);
    std::istringstream in(script);
    std::ostringstream out;
    svc.run_stream(in, out);
    return out.str();
}

}  // namespace

TEST_CASE("price parsing converts to exact integer cents without double") {
    std::int64_t c = 0;
    CHECK(team::parse_cents("50.1", c));
    CHECK(c == 5010);
    CHECK(team::parse_cents("579.00", c));
    CHECK(c == 57900);
    CHECK(team::parse_cents("600", c));
    CHECK(c == 60000);
    CHECK(team::parse_cents("0.05", c));
    CHECK(c == 5);
    CHECK(team::format_cents(5010) == "50.10");
    CHECK(team::format_cents(5) == "0.05");

    CHECK_FALSE(team::parse_cents("", c));
    CHECK_FALSE(team::parse_cents("-5.00", c));
    CHECK_FALSE(team::parse_cents("10.", c));
    CHECK_FALSE(team::parse_cents("10.123", c));
    CHECK_FALSE(team::parse_cents("abc", c));
}

TEST_CASE("p2_demo sample matches expected output exactly") {
    std::ifstream in("samples/p2_demo.txt");
    std::ifstream exp("samples/p2_demo.expected.txt");
    REQUIRE(in.good());
    REQUIRE(exp.good());

    team::StlAlertService svc(10, false);
    std::ostringstream actual;
    svc.run_stream(in, actual);

    std::ostringstream expected;
    expected << exp.rdbuf();

    CHECK(strip_stats(actual.str()) == expected.str());
    CHECK(actual.str().find("STATS prices:") != std::string::npos);
}

TEST_CASE("quiet mode prints only STATS lines") {
    const std::string script =
        "SUB alice news/sports\n"
        "PUB news/sports/soccer Goal\n"
        "INBOX alice\n"
        "STATS\n";
    std::string out = run_script(script, 10, true);
    CHECK(strip_stats(out).empty());
    CHECK(out.find("STATS topics:") != std::string::npos);
    CHECK(out.find("STATS prices:") != std::string::npos);
}

TEST_CASE("inbox retains only last K alerts while history keeps all") {
    const std::string script =
        "SUB alice news\n"
        "PUB news m1\n"
        "PUB news m2\n"
        "PUB news m3\n"
        "INBOX alice\n"
        "HISTORY alice\n";
    std::string out = run_script(script, 2, false);
    CHECK(out.find("ALERT D1 00:00 alice news m1\nALERT D1 00:00 alice news m2\nALERT D1 00:00 alice news m3\n"
                   "ALERT D1 00:00 alice news m2\nALERT D1 00:00 alice news m3\nEND\n") != std::string::npos);
    CHECK(out.find("D1 00:00 SUB news\n"
                   "D1 00:00 ALERT news m1\n"
                   "D1 00:00 ALERT news m2\n"
                   "D1 00:00 ALERT news m3\nEND\n") != std::string::npos);
}

TEST_CASE("price alerts fire highest threshold first and are removed") {
    const std::string script =
        "SUB u1 items/a below 500.00\n"
        "SUB u2 items/a below 600.00\n"
        "PRICE items/a 600.00\n"
        "PRICE items/a 450.00\n"
        "PRICE items/a 400.00\n"
        "PRICE items/a 550.00\n"
        "PRICE items/a 499.99\n";
    std::string out = run_script(script);
    const std::string expected =
        "OK SUB u1 items/a\n"
        "OK SUB u2 items/a\n"
        "ALERT D1 00:00 u2 items/a price 450.00 below 600.00\n"
        "ALERT D1 00:00 u1 items/a price 450.00 below 500.00\n";
    CHECK(out == expected);
}

TEST_CASE("unsub discards held publications and resubscribe uses fresh id") {
    const std::string script =
        "SUB dave news every 2\n"
        "PUB news msg1\n"
        "UNSUB dave news\n"
        "TICK 120\n"
        "SUB dave news every 2\n"
        "PUB news msg2\n"
        "TICK 120\n";
    std::string out = run_script(script);
    const std::string expected =
        "OK SUB dave news\n"
        "OK UNSUB dave news\n"
        "TIME D1 02:00\n"
        "OK SUB dave news\n"
        "ALERT D1 04:00 dave news msg2\n"
        "TIME D1 04:00\n";
    CHECK(out == expected);
}

TEST_CASE("topic prefix respects slash boundaries and most-specific mode wins") {
    const std::string script =
        "SUB bob news\n"
        "SUB bob news/sports at 08:00\n"
        "PUB news/sportscar Fast car\n"
        "PUB news/sports/soccer Match\n"
        "TICK 480\n";
    std::string out = run_script(script);
    const std::string expected =
        "OK SUB bob news\n"
        "OK SUB bob news/sports\n"
        "ALERT D1 00:00 bob news/sportscar Fast car\n"
        "ALERT D1 08:00 bob news/sports/soccer Match\n"
        "TIME D1 08:00\n";
    CHECK(out == expected);
}

TEST_CASE("main, alt, and stl implementations produce identical output on p2_demo and price storms") {
    auto run_all = [](const std::string& input) {
        std::ostringstream out_main, out_alt, out_stl;
        {
            std::istringstream in(input);
            team::MainAlertService svc(10, false);
            svc.run_stream(in, out_main);
        }
        {
            std::istringstream in(input);
            team::AltAlertService svc(10, false);
            svc.run_stream(in, out_alt);
        }
        {
            std::istringstream in(input);
            team::StlAlertService svc(10, false);
            svc.run_stream(in, out_stl);
        }
        CHECK(strip_stats(out_main.str()) == strip_stats(out_stl.str()));
        CHECK(strip_stats(out_alt.str()) == strip_stats(out_stl.str()));
    };

    std::ifstream demo_file("samples/p2_demo.txt");
    REQUIRE(demo_file.good());
    std::ostringstream demo_buf;
    demo_buf << demo_file.rdbuf();
    run_all(demo_buf.str());

    const std::string storm =
        "SUB u1 items/p below 900.00\n"
        "SUB u2 items/p below 800.00\n"
        "SUB u3 items/p below 850.00\n"
        "SUB u4 items/p below 850.00\n"
        "SUB u5 items/p below 700.00\n"
        "SUB u1 news\n"
        "SUB u1 news/sports at 08:00\n"
        "SUB u2 news/sports\n"
        "PUB news/sportscar Fast\n"
        "PUB news/sports/soccer/finals Goal\n"
        "UNSUB u2 news/sports\n"
        "PUB news/sports/basketball Tipoff\n"
        "TICK 480\n"
        "PRICE items/p 950.00\n"
        "PRICE items/p 820.00\n"
        "UNSUB u3 items/p\n"
        "PRICE items/p 860.00\n"
        "PRICE items/p 650.00\n"
        "INBOX u1\n"
        "INBOX u4\n"
        "HISTORY u3\n"
        "STATS\n";
    run_all(storm);
}

TEST_CASE("invalid commands report errors without mutating state") {
    const std::string script =
        "FOO bar\n"
        "SUB alice /leading\n"
        "SUB alice trailing/\n"
        "SUB alice bad//path\n"
        "SUB alice news every 0\n"
        "SUB alice news at 24:00\n"
        "SUB alice items/p below -5.00\n"
        "SUB alice items/p below 10.123\n"
        "SUB alice news below 10 at 08:00\n"
        "PUB news\n"
        "PRICE items/p abc\n"
        "TICK -10\n"
        "UNSUB ghost news\n"
        "INBOX ghost\n"
        "HISTORY ghost\n"
        "TICK 0\n";
    team::MainAlertService svc(10, false);
    std::istringstream in(script);
    std::ostringstream out;
    svc.run_stream(in, out);

    const std::string expected =
        "ERROR unknown command\n"
        "ERROR bad command\n"
        "ERROR bad command\n"
        "ERROR bad command\n"
        "ERROR bad command\n"
        "ERROR bad command\n"
        "ERROR bad command\n"
        "ERROR bad command\n"
        "ERROR bad command\n"
        "ERROR bad command\n"
        "ERROR bad command\n"
        "ERROR bad command\n"
        "ERROR not subscribed ghost news\n"
        "END\n"
        "END\n"
        "TIME D1 00:00\n";
    CHECK(out.str() == expected);
    CHECK(svc.now_minutes() == 0);
}

TEST_CASE("scheduled boundary rules: at current time waits for tomorrow, same-minute ties, huge tick") {
    const std::string script =
        "TICK 480\n"
        "SUB alice news at 08:00\n"
        "SUB bob news every 2\n"
        "SUB carol news every 2\n"
        "PUB news first\n"
        "TICK 120\n"
        "PUB news after_fire\n"
        "TICK 1440\n"
        "INBOX bob 0\n"
        "HISTORY bob 0\n"
        "HISTORY bob 100\n";
    team::MainAlertService svc(10, false);
    std::istringstream in(script);
    std::ostringstream out;
    svc.run_stream(in, out);

    const std::string expected =
        "TIME D1 08:00\n"
        "OK SUB alice news\n"
        "OK SUB bob news\n"
        "OK SUB carol news\n"
        "ALERT D1 10:00 bob news first\n"
        "ALERT D1 10:00 carol news first\n"
        "TIME D1 10:00\n"
        "ALERT D1 12:00 bob news after_fire\n"
        "ALERT D1 12:00 carol news after_fire\n"
        "ALERT D2 08:00 alice news first\n"
        "ALERT D2 08:00 alice news after_fire\n"
        "TIME D2 10:00\n"
        "END\n"
        "END\n"
        "D1 08:00 SUB news every 2\n"
        "D1 10:00 ALERT news first\n"
        "D1 12:00 ALERT news after_fire\n"
        "END\n";
    CHECK(out.str() == expected);
}

TEST_CASE("replacement cancels pending delivery and selects new mode") {
    auto out = run_script("SUB alice news every 2\nPUB news old\nSUB alice news\nPUB news new\nTICK 120\n");
    CHECK(out == "OK SUB alice news\nOK SUB alice news\nALERT D1 00:00 alice news new\nTIME D1 02:00\n");
}

TEST_CASE("equal deadlines follow scheduling order") {
    auto out = run_script("SUB a news every 2\nTICK 60\nSUB b news every 3\nTICK 60\nPUB news message\nTICK 120\n");
    CHECK(out.find("ALERT D1 04:00 b news message\nALERT D1 04:00 a news message\n") != std::string::npos);
}

TEST_CASE("topic removal uses an index and preserves moved subscriptions") {
    team::StlTopicStore<> topics;
    for (std::uint64_t i = 0; i < 10000; ++i) topics.add_subscription("news", i);
    topics.reset_stats();
    CHECK(topics.remove_subscription("news", 9998));
    CHECK(topics.stats().visits < 10);
    CHECK(topics.remove_subscription("news", 9999));
    CHECK_FALSE(topics.remove_subscription("news", 9998));
    CHECK(topics.subscriber_count() == 9998);
}
