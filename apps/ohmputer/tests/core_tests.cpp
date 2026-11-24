// Host tests for the OhmPuter core. Non-zero exit on failure.
#include "eseries.h"
#include "marking.h"
#include "session.h"
#include "si.h"
#include "tools.h"

#include <cmath>
#include <cstdio>
#include <string>

using namespace ohm;

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool ok, const std::string& what) {
    ++g_checks;
    if (!ok) {
        ++g_failures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

void si(const std::string& text, double expected) {
    const auto v = parse_si(text);
    check(v && std::fabs(*v - expected) <= 1e-9 * std::fabs(expected) + 1e-18,
          "parse_si(" + text + ") = " + (v ? std::to_string(*v) : "none") + ", expected " + std::to_string(expected));
}

void no_si(const std::string& text) { check(!parse_si(text), "parse_si(" + text + ") should fail"); }

bool has_reading(const std::vector<Reading>& rs, char kind, const std::string& part) {
    for (const auto& r : rs) {
        if (r.kind == kind && r.value.find(part) != std::string::npos) return true;
    }
    return false;
}

void reads(const std::string& code, char kind, const std::string& part) {
    const auto rs = decode_marking(code);
    std::string all;
    for (const auto& r : rs) all += std::string(1, r.kind) + ":" + r.value + "; ";
    check(has_reading(rs, kind, part), code + " -> expected " + kind + " '" + part + "', got " + all);
}

void not_reads(const std::string& code, char kind) {
    for (const auto& r : decode_marking(code)) {
        check(r.kind != kind, code + " should not read as " + kind + " (" + r.value + ")");
    }
}

const char* kOhmSign = "\xCE\xA9";
const std::string kMicro = "\xC2\xB5";

void test_si() {
    si("4k7", 4700);
    si("4.7k", 4700);
    si("4K7", 4700);
    si("100n", 100e-9);
    si("2u2", 2.2e-6);
    si("u47", 0.47e-6);
    si("4R7", 4.7);
    si("R47", 0.47);
    si("47R", 47);
    si("1M", 1e6);
    si("1m", 1e-3);
    si("10mA", 0.01);
    si("1e3", 1000);
    si("2.5e-3", 0.0025);
    si("-5", -5);
    si("100nF", 100e-9);
    si("10uH", 10e-6);
    si("50Hz", 50);
    si("314Ah", 314);
    si("51.2V", 51.2);
    si("1k5" + std::string(kOhmSign), 1500);
    si("4.7 " + kMicro + "F", 4.7e-6);
    si("90%", 90);
    no_si("");
    no_si("abc");
    no_si("4.7k7");
    no_si("4kx");
    check(format_si(4700, "V") == "4.7 kV", "format 4.7k: " + format_si(4700, "V"));
    check(format_si(1e-7, "F") == "100 nF", "format 100n");
    check(format_si(999.96, "V") == "1 kV", "format rounds up a prefix: " + format_si(999.96, "V"));
    check(format_si(0.0047, "A") == "4.7 mA", "format mA");
    check(format_si(2.2e-6, "F") == "2.2 " + kMicro + "F", "format micro");
    check(format_si(12.3456, "V", 3) == "12.3 V", "format 3 digits");
    check(format_si(0, "V") == "0 V", "format 0");
    check(format_plain(0.0473) == "0.0473", "plain small");
}

void test_eseries() {
    check(e96().size() == 96, "E96 has 96 values");
    check(series_values(Series::E24).size() == 24, "E24 size");
    check(series_values(Series::E12).size() == 12, "E12 size");
    check(series_values(Series::E6).size() == 6, "E6 size");
    check(series_values(Series::E48).size() == 48, "E48 size");
    check(series_values(Series::E12)[8] == 470, "E12 has 470");
    check(std::fabs(nearest(4600, Series::E12) - 4700) < 1e-6, "nearest 4600 E12");
    check(std::fabs(nearest(1050, Series::E24) - 1000) < 1e-6 || std::fabs(nearest(1050, Series::E24) - 1100) < 1e-6,
          "nearest 1050");
    check(std::fabs(nearest(160, Series::E12, +1) - 180) < 1e-6, "nearest up");
    check(std::fabs(nearest(160, Series::E12, -1) - 150) < 1e-6, "nearest down");
    check(std::fabs(nearest(9.9, Series::E12) - 10) < 1e-9, "nearest crosses decade");
    check(in_series(4700, Series::E12) && !in_series(4800, Series::E24), "in_series");
    const auto p = divider_pair(3.3 / 5.0, Series::E24);
    check(p.r1 > 0 && std::fabs(p.ratio / 0.66 - 1) < 0.01, "divider pair within 1%");
    const auto exact = divider_pair(0.5, Series::E12);
    check(std::fabs(exact.ratio - 0.5) < 1e-12 && exact.r1 == exact.r2, "divider 1:1");
}

void test_bands() {
    using C = Color;
    auto r = decode_bands({C::Yellow, C::Violet, C::Red, C::Gold});
    check(r.ok && std::fabs(r.ohms - 4700) < 1e-6 && std::fabs(r.tolerance - 0.05) < 1e-12, "yel vio red gold");
    r = decode_bands({C::Brown, C::Black, C::Black, C::Red, C::Brown});
    check(r.ok && std::fabs(r.ohms - 10000) < 1e-6 && std::fabs(r.tolerance - 0.01) < 1e-12, "5-band 10k 1%");
    r = decode_bands({C::Brown, C::Black, C::Black, C::Red, C::Brown, C::Red});
    check(r.ok && r.tempco == 50, "6-band tempco");
    r = decode_bands({C::Yellow, C::Violet, C::Gold, C::Gold});
    check(r.ok && std::fabs(r.ohms - 4.7) < 1e-9, "gold multiplier");
    r = decode_bands({C::Red, C::Red, C::Brown});
    check(r.ok && std::fabs(r.ohms - 220) < 1e-9 && std::fabs(r.tolerance - 0.2) < 1e-12, "3-band 20%");
    r = decode_bands({C::Gold, C::Red, C::Violet, C::Yellow});
    check(r.ok && r.reversed && std::fabs(r.ohms - 4700) < 1e-6, "reversed reading");
    r = decode_bands({C::Black});
    check(r.ok && r.ohms == 0, "zero ohm");
    r = decode_bands({C::Red, C::Red});
    check(!r.ok, "2 bands invalid");
    r = decode_bands({C::Red, C::Red, C::Red, C::Black});
    check(!r.ok, "black is not a tolerance");

    auto e = encode_bands(4700, 2, 0.05);
    check(e == std::vector<Color>({C::Yellow, C::Violet, C::Red, C::Gold}), "encode 4k7");
    e = encode_bands(4.7, 2, 0.05);
    check(e.size() == 4 && e[2] == C::Gold, "encode 4.7 gold");
    e = encode_bands(10000, 3, 0.01);
    check(e == std::vector<Color>({C::Brown, C::Black, C::Black, C::Red, C::Brown}), "encode 10k 5-band");
    e = encode_bands(4990, 2, 0.05);
    check(e.empty(), "4.99k needs 3 digits");
    e = encode_bands(220, 2, 0.2);
    check(e.size() == 3, "20% has no tolerance band");
    // every E24 value round-trips through 4 bands
    for (int m : series_values(Series::E24)) {
        const double v = m * 10.0;
        const auto b = encode_bands(v, 2, 0.05);
        const auto back = decode_bands(b);
        check(back.ok && std::fabs(back.ohms - v) < 1e-6, "round trip " + std::to_string(v));
    }
    check(*color_from_key('4') == C::Yellow && *color_from_key('g') == C::Gold && !color_from_key('x'), "keys");
}

void test_markings() {
    const std::string k = std::string(" k") + kOhmSign;
    reads("103", 'R', "10" + k);
    reads("103", 'C', "10 nF");
    reads("103", 'L', "10 mH");
    reads("104", 'C', "100 nF = 0.1 " + kMicro + "F");
    reads("472", 'R', "4.7" + k);
    reads("229", 'C', "2.2 pF");
    reads("1002", 'R', "10" + k);
    reads("4992", 'R', "49.9" + k);
    reads("4R7", 'R', "4.7 " + std::string(kOhmSign));
    reads("4R7", 'L', "4.7 " + kMicro + "H");
    reads("R47", 'R', "470 m");
    reads("4K7", 'R', "4.7" + k);
    reads("1M0", 'R', "1 M");
    reads("4n7", 'C', "4.7 nF");
    reads("2p2", 'C', "2.2 pF");
    reads("01C", 'R', "10" + k);
    reads("68X", 'R', "49.9 ");
    reads("47R", 'R', "47 " + std::string(kOhmSign));
    reads("104K", 'C', "\xC2\xB1" "10 %");
    not_reads("104K", 'R');
    reads("2A104J", 'C', "100 V");
    reads("2A104J", 'C', "\xC2\xB1" "5 %");
    reads("1H473", 'C', "47 nF");
    reads("1H473", 'C', "50 V");
    reads("A106", 'C', "10 " + kMicro + "F  10 V");
    reads("4K7J", 'R', "\xC2\xB1" "5 %");
    reads("000", 'R', "0 ");
    reads("0R", 'R', "link");
    reads("M7", 'D', "1N4007");
    not_reads("M7", 'R');
    reads("1am", 'Q', "MMBT3904");
    reads("702", 'Q', "2N7002");
    reads("702", 'R', "7" + k);
    reads("4.7k", 'R', "4.7" + k);
    reads("100nF", 'C', "100 nF");
    reads("10uH", 'L', "10 " + kMicro + "H");
    reads("47", 'R', "47 ");
    check(decode_marking("").empty(), "empty");
    check(decode_marking("xyz").empty(), "nonsense");
}

double num(const Solution& s, size_t i) { return s.values[i] ? *s.values[i] : NAN; }

void test_tools() {
    const auto& t = tools();
    check(t.size() == 6, "tool count");
    {
        Form f(t[0]);  // V I R P
        f.set_text(0, "12");
        f.set_text(2, "1k");
        auto s = f.solve(Series::E24);
        check(std::fabs(num(s, 1) - 0.012) < 1e-12 && std::fabs(num(s, 3) - 0.144) < 1e-12, "ohm V,R");
        f.set_text(3, "1");  // third input clears the oldest (V)
        check(!f.is_set(0) && f.is_set(2) && f.is_set(3), "oldest cleared");
        s = f.solve(Series::E24);
        check(std::fabs(num(s, 0) - std::sqrt(1000.0)) < 1e-9, "ohm R,P");
        f.set_text(1, "abc");
        s = f.solve(Series::E24);
        check(!s.error.empty(), "bad text error");
    }
    {
        Form f(t[1]);  // Vin R1 R2 Vout I
        f.set_text(0, "5");
        f.set_text(1, "10k");
        f.set_text(2, "10k");
        auto s = f.solve(Series::E24);
        check(std::fabs(num(s, 3) - 2.5) < 1e-12 && std::fabs(num(s, 4) - 0.00025) < 1e-12, "divider out");
        Form g(t[1]);
        g.set_text(0, "5");
        g.set_text(3, "3.3");
        s = g.solve(Series::E24);
        check(s.notes.size() == 2 && s.notes[0].find("R1") != std::string::npos, "divider suggestion");
        g.set_text(3, "6");
        check(!g.solve(Series::E24).error.empty(), "Vout above Vin");
        Form h(t[1]);
        h.set_text(0, "12");
        h.set_text(2, "10k");
        h.set_text(3, "3");
        s = h.solve(Series::E24);
        check(std::fabs(num(s, 1) - 30000) < 1e-6, "divider R1");
    }
    {
        Form f(t[2]);  // LED
        f.set_text(0, "5");
        f.set_text(1, "2");
        f.set_text(2, "20m");
        auto s = f.solve(Series::E24);
        check(std::fabs(num(s, 3) - 150) < 1e-9, "led R");
        check(std::fabs(num(s, 4) - 0.06) < 1e-12 && std::fabs(num(s, 5) - 0.04) < 1e-12, "led powers");
        check(!s.notes.empty() && s.notes[0].find("150") != std::string::npos, "led series note: " + (s.notes.empty() ? "" : s.notes[0]));
    }
    {
        Form f(t[3]);  // RC
        f.set_text(0, "10k");
        f.set_text(1, "100n");
        auto s = f.solve(Series::E24);
        check(std::fabs(num(s, 2) - 1e-3) < 1e-15 && std::fabs(num(s, 3) - 159.1549430918953) < 1e-9, "rc");
        Form g(t[3]);
        g.set_text(1, "100n");
        g.set_text(3, "1k");
        s = g.solve(Series::E24);
        check(std::fabs(num(s, 0) - 1591.549430918953) < 1e-6, "rc R from fc");
        g.set_text(2, "1m");  // tau + fc after clearing C
        check(!g.is_set(1), "rc clears C");
        check(!g.solve(Series::E24).error.empty(), "tau and fc conflict");
    }
    {
        Form f(t[4]);  // LC
        f.set_text(0, "10u");
        f.set_text(1, "100n");
        auto s = f.solve(Series::E24);
        check(std::fabs(num(s, 2) - 159154.9430918953) < 1e-3, "lc f0");
        check(std::fabs(num(s, 3) - 10) < 1e-9, "lc Z0");
    }
    {
        Form f(t[5]);  // battery
        f.restore({"314", "51.2", "500", "90", "90"}, {});
        auto s = f.solve(Series::E24);
        const double usable = 314 * 51.2 * 0.9 * 0.9;
        check(std::fabs(num(s, 5) - usable) < 1e-6, "battery usable");
        check(std::fabs(num(s, 7) - usable / 500) < 1e-9, "battery runtime");
        check(std::fabs(num(s, 6) - 500 / 0.9 / 51.2) < 1e-9, "battery current");
    }
    check(resistor_rating(0.06) == "1/8 W", "rating 1/8");
    check(resistor_rating(0.1) == "1/4 W", "rating 1/4");
    check(format_hours(1.5) == "1 h 30 min", "hours");
    check(format_hours(0.25) == "15 min", "minutes");
    check(format_hours(72) == "3 days", "days");
}

void test_session() {
    Session s;
    s.series = Series::E96;
    s.mark_input = "4R7";
    s.bands = {Color::Yellow, Color::Violet, Color::Red, Color::Gold};
    s.band_from_value = true;
    s.band_value = "4k7";
    s.tool = 2;
    s.forms[0].set_text(0, "12");
    s.forms[0].set_text(2, "1k");
    Session t;
    t.deserialize(s.serialize());
    check(t.series == Series::E96 && t.mark_input == "4R7" && t.tool == 2, "session scalars");
    check(t.bands == s.bands && t.band_from_value && t.band_value == "4k7", "session bands");
    check(t.forms[0].text(0) == "12" && t.forms[0].text(2) == "1k" && t.forms[0].order() == s.forms[0].order(),
          "session form");
    check(t.forms[5].text(0) == "100", "battery defaults survive");
    t.forms[0].set_text(3, "1");
    check(!t.forms[0].is_set(0), "restored order drives clearing");
}

}  // namespace

int main() {
    test_si();
    test_eseries();
    test_bands();
    test_markings();
    test_tools();
    test_session();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
