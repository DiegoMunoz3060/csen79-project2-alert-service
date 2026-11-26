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
    CHECK(out.find("ALERT D0 00:00 alice news m1\nALERT D0 00:00 alice news m2\nALERT D0 00:00 alice news m3\n"
                   "ALERT D0 00:00 alice news m2\nALERT D0 00:00 alice news m3\nEND\n") != std::string::npos);
    CHECK(out.find("D0 00:00 SUB news\n"
                   "D0 00:00 ALERT news m1\n"
                   "D0 00:00 ALERT news m2\n"
                   "D0 00:00 ALERT news m3\nEND\n") != std::string::npos);
}

TEST_CASE("price alerts fire highest threshold first and re-arm after rising") {
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
        "ALERT D0 00:00 u2 items/a price 450.00 below 600.00\n"
        "ALERT D0 00:00 u1 items/a price 450.00 below 500.00\n"
        "ALERT D0 00:00 u1 items/a price 499.99 below 500.00\n";
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
        "TIME D0 02:00\n"
        "OK SUB dave news\n"
        "ALERT D0 04:00 dave news msg2\n"
        "TIME D0 04:00\n";
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
        "ALERT D0 00:00 bob news/sportscar Fast car\n"
        "ALERT D0 08:00 bob news/sports/soccer Match\n"
        "TIME D0 08:00\n";
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
        "PRICE items/p 950.00\n"
        "PRICE items/p 820.00\n"
        "UNSUB u3 items/p\n"
        "PRICE items/p 860.00\n"
        "PRICE items/p 650.00\n"
        "INBOX u4\n"
        "HISTORY u3\n"
        "STATS\n";
    run_all(storm);
}
