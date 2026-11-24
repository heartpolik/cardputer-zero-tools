// Host tests for the calculator core. No framework: run the binary, non-zero exit on failure.
#include "engine.h"
#include "expr.h"
#include "format.h"
#include "line_editor.h"
#include "plot_math.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

using namespace calc;

namespace {

int g_failures = 0;
int g_checks = 0;

void fail(const std::string& what) {
    ++g_failures;
    std::printf("FAIL: %s\n", what.c_str());
}

double eval(const std::string& src, AngleMode mode = AngleMode::Rad) {
    Env env;
    env.angle = mode;
    return env.eval(parse_statement(src).body);
}

void near(const std::string& src, double expected, AngleMode mode = AngleMode::Rad, double tol = 1e-9) {
    ++g_checks;
    try {
        const double v = eval(src, mode);
        const double scale = std::max(1.0, std::fabs(expected));
        if (!(std::fabs(v - expected) <= tol * scale)) {
            fail(src + " = " + std::to_string(v) + ", expected " + std::to_string(expected));
        }
    } catch (const CalcError& e) {
        fail(src + " threw: " + e.what());
    }
}

void throws(const std::string& src, const std::string& message_part = "", size_t pos = std::string::npos) {
    ++g_checks;
    try {
        (void)eval(src);
        fail(src + " should throw");
    } catch (const CalcError& e) {
        if (!message_part.empty() && std::string(e.what()).find(message_part) == std::string::npos) {
            fail(src + " threw '" + e.what() + "', expected '" + message_part + "'");
        }
        if (pos != std::string::npos && e.pos() != pos) {
            fail(src + " error pos " + std::to_string(e.pos()) + ", expected " + std::to_string(pos));
        }
    }
}

void fmt(double v, NumberFormat f, const std::string& expected) {
    ++g_checks;
    const auto s = format_number(v, f);
    if (s != expected) fail("format(" + std::to_string(v) + ") = '" + s + "', expected '" + expected + "'");
}

void check(bool cond, const std::string& what) {
    ++g_checks;
    if (!cond) fail(what);
}

void test_arithmetic() {
    near("1+2*3", 7);
    near("(1+2)*3", 9);
    near("2^3^2", 512);  // right associative
    near("-2^2", -4);
    near("2^-1", 0.5);
    near("10/4", 2.5);
    near("7%3", 1);
    near("2**10", 1024);
    near("5!", 120);
    near("3!^2", 36);
    near("0.5!", std::tgamma(1.5));
    near("1e3", 1000);
    near("1.5e-3", 0.0015);
    near(".5", 0.5);
    near("0x1F", 31);
    near("0b1010", 10);
    near("--3", 3);
    near("+3", 3);
}

void test_implicit_multiplication() {
    near("2pi", 2 * M_PI);
    near("2(3+4)", 14);
    near("(1+1)(2+2)", 8);
    near("2e", 2 * M_E);
    near("3sqrt(4)", 6);
    near("2pi^2", 2 * M_PI * M_PI);  // power binds tighter
    near("-2pi", -2 * M_PI);
}

void test_functions() {
    near("sqrt(16)", 4);
    near("cbrt(-27)", -3);
    near("root(-8, 3)", -2);
    near("abs(-3)", 3);
    near("ln(e)", 1);
    near("log(1000)", 3);
    near("log(2, 8)", 3);
    near("log2(1024)", 10);
    near("exp(0)", 1);
    near("min(3, 1, 2)", 1);
    near("max(3, 1, 2)", 3);
    near("gcd(12, 18)", 6);
    near("lcm(4, 6)", 12);
    near("ncr(5, 2)", 10);
    near("npr(5, 2)", 20);
    near("round(2.567, 2)", 2.57);
    near("mod(-7, 3)", 2);
    near("hypot(3, 4)", 5);
    near("sign(-2)", -1);
    near("floor(-1.5)", -2);
}

void test_trig() {
    near("sin(pi/2)", 1);
    near("sin(pi)", 0, AngleMode::Rad, 1e-15);
    near("sin(90)", 1, AngleMode::Deg);
    near("sin(180)", 0, AngleMode::Deg, 0);
    near("cos(90)", 0, AngleMode::Deg, 0);
    near("tan(45)", 1, AngleMode::Deg);
    near("asin(1)", 90, AngleMode::Deg);
    near("atan2(1, 1)", 45, AngleMode::Deg);
    near("torad(180)", M_PI, AngleMode::Deg);
    near("todeg(pi)", 180);
    check(std::isnan(eval("tan(90)", AngleMode::Deg)), "tan(90deg) is undefined");
}

void test_special_forms() {
    near("der(x^2, 3)", 6, AngleMode::Rad, 0);
    near("der(x^3, 2)", 12, AngleMode::Rad, 0);
    near("der(cos(x), 0)", 0, AngleMode::Rad, 0);
    near("der(sin(x), 0)", 1, AngleMode::Rad, 1e-7);
    near("integ(x^2, 0, 3)", 9, AngleMode::Rad, 1e-9);
    near("integ(sin(x), 0, pi)", 2, AngleMode::Rad, 1e-9);
    near("solve(x^2 - 2, 1)", std::sqrt(2.0));
    near("solve(cos(x) - x, 0, 1)", 0.7390851332151607);
    near("sum(x, 1, 100)", 5050);
    throws("solve(x^2 + 1, 0)", "no root");
    throws("der(x)", "usage");
}

void test_errors() {
    throws("1+", "incomplete", 2);
    throws("(1+2", "missing ')'", 0);
    throws("1+2)", "unmatched ')'", 3);
    throws("2 3", "unexpected", 2);
    throws("foo", "unknown name 'foo'", 0);
    throws("foo(1, 2)", "unknown function", 0);
    throws("sin(1, 2)", "expects 1");
    throws("sin", "needs arguments");
    throws("1/0", "division by zero", 1);
    throws("2 $ 3", "unexpected '$'", 2);
    throws("gcd(1.5, 2)", "integer expected");
    throws("0x", "bad hex");
}

void test_statements() {
    auto st = parse_statement("a = 2 + 3");
    check(st.kind == Statement::Kind::AssignVariable && st.name == "a", "assignment parsed");
    check(st.body_source == "2 + 3", "assignment body source");

    st = parse_statement("f(x, y) = x*y");
    check(st.kind == Statement::Kind::DefineFunction && st.params.size() == 2, "function parsed");
    check(st.body_source == "x*y", "function body source");

    st = parse_statement("2+2=");
    check(st.kind == Statement::Kind::Expression, "trailing = tolerated");

    bool threw = false;
    try {
        parse_statement("pi = 3");
    } catch (const CalcError&) {
        threw = true;
    }
    check(threw, "constants are reserved");

    threw = false;
    try {
        parse_statement("1 = 2");
    } catch (const CalcError&) {
        threw = true;
    }
    check(threw, "assign to number rejected");
}

void test_engine() {
    Engine e;
    auto r = e.submit("2+3");
    check(r.ok && r.text == "5", "submit 2+3");
    r = e.submit("ans*2");
    check(r.ok && r.value == 10, "ans works");
    r = e.submit("r = 4");
    check(r.ok && r.text == "r = 4", "assignment output");
    r = e.submit("area(r) = pi r^2");
    check(r.ok && r.text == "area(r) defined", "define output: " + r.text);
    r = e.submit("area(2)");
    check(r.ok && std::fabs(r.value - 4 * M_PI) < 1e-12, "user function call");
    r = e.submit("g(x) = h(x) + 1");
    check(!r.ok && r.text.find("unknown function 'h'") != std::string::npos, "undefined dependency rejected");
    check(e.env.funcs.count("g") == 0, "rejected function not stored");
    r = e.submit("fact(r)");
    check(r.ok && r.value == 24, "variable as argument");
    r = e.submit("3r");
    check(r.ok && r.value == 12, "implicit mult with variable");
    r = e.submit("r(2)");
    check(r.ok && r.value == 8, "variable followed by parenthesis multiplies");
    r = e.submit("1/0");
    check(!r.ok && r.error_pos == 1, "error result with position");
    check(e.history.size() == 8, "history size " + std::to_string(e.history.size()));
    check(e.history.back().input == "r(2)", "errors are not stored in history");

    auto p = e.preview("r + 1");
    check(p.ok && p.text == "5", "preview");
    check(e.history.size() == 8, "preview has no side effects");
    p = e.preview("r +");
    check(!p.ok, "preview of incomplete input");

    e.env.angle = AngleMode::Deg;
    e.format = NumberFormat::Eng;
    e.plots[0] = "sin(x)";
    e.plot_enabled[1] = false;
    const auto saved = e.serialize();

    Engine loaded;
    loaded.deserialize(saved);
    check(loaded.env.angle == AngleMode::Deg, "angle restored");
    check(loaded.format == NumberFormat::Eng, "format restored");
    check(loaded.env.vars.at("r") == 4, "var restored");
    check(loaded.env.funcs.count("area") == 1, "function restored");
    check(loaded.history.size() == e.history.size(), "history restored");
    check(loaded.plots[0] == "sin(x)" && !loaded.plot_enabled[1], "plots restored");
    r = loaded.submit("area(1)");
    check(r.ok && std::fabs(r.value - M_PI) < 1e-12, "restored function works");
}

void test_plots() {
    Engine e;
    e.submit("f(x) = x^2");
    auto p = e.compile_plot("f");
    check(p && e.eval_plot(*p, 3) == 9, "plot bare function name");
    p = e.compile_plot("y = 2x + 1");
    check(p && e.eval_plot(*p, 2) == 5, "plot y = expr");
    p = e.compile_plot("sqrt(x)");
    check(p && std::isnan(e.eval_plot(*p, -1)), "domain error is NaN");
    p = e.compile_plot("1/x");
    check(p && std::isnan(e.eval_plot(*p, 0)), "division by zero is NaN");
    std::string err;
    p = e.compile_plot("foo(x)", &err);
    check(!p && err.find("unknown function") != std::string::npos, "plot unknown function: " + err);
}

void test_line_editor() {
    LineEditor ed;
    ed.insert("sin(x)");
    check(ed.cursor() == 6, "cursor after insert");
    ed.left();
    ed.backspace();
    check(ed.text() == "sin()" && ed.cursor() == 4, "backspace inside: " + ed.text());
    ed.insert("pi");
    check(ed.text() == "sin(pi)", "insert inside");
    ed.home();
    ed.del();
    check(ed.text() == "in(pi)" && ed.cursor() == 0, "delete at home");
    ed.backspace();
    check(ed.text() == "in(pi)", "backspace at home is no-op");
    ed.end();
    ed.del();
    check(ed.text() == "in(pi)", "delete at end is no-op");

    check(prepare_input("*2") == "ans*2", "leading * uses ans");
    check(prepare_input("  ^2") == "  ans^2", "leading ^ uses ans");
    check(prepare_input("-2") == "-2", "leading - is a sign");
    check(prepare_input("sin(pi/2") == "sin(pi/2)", "auto close paren");
    check(prepare_input("sqrt(sqrt(16") == "sqrt(sqrt(16))", "auto close two parens");
    check(prepare_input("sin(1=") == "sin(1)=", "close before trailing =");
    check(prepare_input("1)") == "1)", "extra close untouched");
}

void test_plot_math() {
    check(nice_step(20, 8) == 2, "nice step 20/8");
    check(nice_step(1, 5) == 0.2, "nice step 1/5");
    check(std::fabs(nice_step(7.3, 6) - 1) < 1e-12, "nice step 7.3/6");
    check(std::fabs(nice_step(0.03, 6) - 0.005) < 1e-15, "nice step 0.03/6");

    auto v = Viewport::square(-10, 10, 320, 160);
    check(v.ymin == -5 && v.ymax == 5, "square viewport");
    v.zoom(2, 0, 0);
    check(v.xmin == -5 && v.xmax == 5 && v.ymin == -2.5, "zoom in around center");
    v.pan(0.5, 0);
    check(v.xmin == 0 && v.xmax == 10, "pan right half a screen");

    const Fn sq = [](double x) { return x * x - 2; };
    auto r = root_near(sq, 1, -10, 10, 320);
    check(r && std::fabs(*r - std::sqrt(2.0)) < 1e-10, "root near 1");
    r = root_near(sq, -3, -10, 10, 320);
    check(r && std::fabs(*r + std::sqrt(2.0)) < 1e-10, "root near -3");
    r = root_near([](double x) { return 1 / x; }, 0.5, -10, 10, 321);
    check(!r, "pole is not a root");
    r = root_near([](double x) { return x * x + 1; }, 0, -10, 10, 320);
    check(!r, "no root");

    bool is_max = false;
    auto e = extremum_near([](double x) { return -(x - 1) * (x - 1) + 3; }, 0, -10, 10, 320, &is_max);
    check(e && std::fabs(*e - 1) < 1e-6 && is_max, "maximum at 1");
    e = extremum_near([](double x) { return std::cos(x); }, 3, -10, 10, 320, &is_max);
    check(e && std::fabs(*e - M_PI) < 1e-10 && !is_max, "cos minimum at pi (refined)");
    e = extremum_near([](double x) { return std::sin(x); }, 1, -10, 10, 320, &is_max);
    check(e && std::fabs(*e - M_PI / 2) < 1e-10 && is_max, "sin maximum at pi/2 (refined)");
}

void test_format() {
    fmt(0, NumberFormat::Auto, "0");
    fmt(5, NumberFormat::Auto, "5");
    fmt(-2.5, NumberFormat::Auto, "-2.5");
    fmt(1.0 / 3, NumberFormat::Auto, "0.333333333333");
    fmt(0.1 + 0.2, NumberFormat::Auto, "0.3");
    fmt(123456789012.0, NumberFormat::Auto, "123456789012");
    fmt(1234567890123.0, NumberFormat::Auto, "1.23456789012e12");
    fmt(1e-10, NumberFormat::Auto, "1e-10");
    fmt(0.000123, NumberFormat::Auto, "0.000123");
    fmt(M_PI, NumberFormat::Sci, "3.14159265359e0");
    fmt(12345, NumberFormat::Sci, "1.2345e4");
    fmt(4700, NumberFormat::Eng, "4.7e3");
    fmt(0.0047, NumberFormat::Eng, "4.7e-3");
    fmt(47, NumberFormat::Eng, "47");
    fmt(999.99999999999999, NumberFormat::Eng, "1e3");
    fmt(std::nan(""), NumberFormat::Auto, "undefined");
    fmt(INFINITY, NumberFormat::Auto, "inf");
    check(format_short(0.5) == "0.5", "short 0.5");
    check(format_short(1000) == "1000", "short 1000");
    check(format_short(1e6) == "1e6", "short 1e6: " + format_short(1e6));
    check(format_short(-0.25) == "-0.25", "short -0.25");
}

}  // namespace

int main() {
    const std::vector<std::pair<const char*, std::function<void()>>> suites = {
        {"arithmetic", test_arithmetic},
        {"implicit multiplication", test_implicit_multiplication},
        {"functions", test_functions},
        {"trig", test_trig},
        {"special forms", test_special_forms},
        {"errors", test_errors},
        {"statements", test_statements},
        {"engine", test_engine},
        {"plots", test_plots},
        {"line editor", test_line_editor},
        {"plot math", test_plot_math},
        {"format", test_format},
    };
    for (const auto& [name, fn] : suites) {
        const int before = g_failures;
        fn();
        std::printf("%-26s %s\n", name, g_failures == before ? "ok" : "FAILED");
    }
    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
