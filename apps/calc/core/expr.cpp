#include "expr.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>

namespace calc {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr int kMaxDepth = 64;

// ---------------------------------------------------------------- lexer

struct Token {
    enum class Kind { Number, Ident, Op, End };
    Kind kind{Kind::End};
    double number{0.0};
    std::string text;  // ident name or operator char
    size_t pos{0};
    size_t end{0};

    bool is_op(char c) const { return kind == Kind::Op && text.size() == 1 && text[0] == c; }
};

bool is_ident_start(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool is_ident_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

std::vector<Token> tokenize(std::string_view src) {
    std::vector<Token> out;
    size_t i = 0;
    while (i < src.size()) {
        const char c = src[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            ++i;
            continue;
        }
        Token t;
        t.pos = i;

        // hex / binary literals
        if (c == '0' && i + 1 < src.size() && (src[i + 1] == 'x' || src[i + 1] == 'X' ||
                                                 src[i + 1] == 'b' || src[i + 1] == 'B')) {
            const bool hex = src[i + 1] == 'x' || src[i + 1] == 'X';
            size_t j = i + 2;
            uint64_t value = 0;
            size_t digits = 0;
            while (j < src.size()) {
                const char d = src[j];
                int v = -1;
                if (hex && std::isxdigit(static_cast<unsigned char>(d))) {
                    v = std::isdigit(static_cast<unsigned char>(d)) ? d - '0'
                                                                    : (std::tolower(d) - 'a' + 10);
                } else if (!hex && (d == '0' || d == '1')) {
                    v = d - '0';
                } else {
                    break;
                }
                value = value * (hex ? 16 : 2) + static_cast<uint64_t>(v);
                ++digits;
                ++j;
            }
            if (digits == 0) {
                throw CalcError(hex ? "bad hex number" : "bad binary number", i);
            }
            t.kind = Token::Kind::Number;
            t.number = static_cast<double>(value);
            t.end = j;
            out.push_back(t);
            i = j;
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(c)) ||
            (c == '.' && i + 1 < src.size() && std::isdigit(static_cast<unsigned char>(src[i + 1])))) {
            size_t j = i;
            while (j < src.size() && std::isdigit(static_cast<unsigned char>(src[j]))) ++j;
            if (j < src.size() && src[j] == '.') {
                ++j;
                while (j < src.size() && std::isdigit(static_cast<unsigned char>(src[j]))) ++j;
            }
            // exponent only when followed by a digit (optionally signed): "2e" is 2*e
            if (j < src.size() && (src[j] == 'e' || src[j] == 'E')) {
                size_t k = j + 1;
                if (k < src.size() && (src[k] == '+' || src[k] == '-')) ++k;
                if (k < src.size() && std::isdigit(static_cast<unsigned char>(src[k]))) {
                    j = k;
                    while (j < src.size() && std::isdigit(static_cast<unsigned char>(src[j]))) ++j;
                }
            }
            t.kind = Token::Kind::Number;
            t.number = std::strtod(std::string(src.substr(i, j - i)).c_str(), nullptr);
            t.end = j;
            out.push_back(t);
            i = j;
            continue;
        }

        if (is_ident_start(c)) {
            size_t j = i;
            while (j < src.size() && is_ident_char(src[j])) ++j;
            t.kind = Token::Kind::Ident;
            t.text = std::string(src.substr(i, j - i));
            t.end = j;
            out.push_back(t);
            i = j;
            continue;
        }

        if (c == '*' && i + 1 < src.size() && src[i + 1] == '*') {  // ** as power
            t.kind = Token::Kind::Op;
            t.text = "^";
            t.end = i + 2;
            out.push_back(t);
            i += 2;
            continue;
        }

        static const std::string_view kOps = "+-*/%^!(),=";
        if (kOps.find(c) != std::string_view::npos) {
            t.kind = Token::Kind::Op;
            t.text = std::string(1, c);
            t.end = i + 1;
            out.push_back(t);
            ++i;
            continue;
        }

        throw CalcError(std::string("unexpected '") + c + "'", i);
    }
    Token end;
    end.kind = Token::Kind::End;
    end.pos = end.end = src.size();
    out.push_back(end);
    return out;
}

// ---------------------------------------------------------------- parser

class Parser {
public:
    Parser(std::vector<Token> tokens, size_t begin, size_t end)
        : tokens_(std::move(tokens)), i_(begin), end_(end) {}

    NodePtr parse_all() {
        auto node = parse_additive();
        if (i_ < end_) {
            const auto& t = tokens_[i_];
            if (t.is_op(')')) throw CalcError("unmatched ')'", t.pos);
            throw CalcError("unexpected '" + token_text(t) + "'", t.pos);
        }
        return node;
    }

private:
    static std::string token_text(const Token& t) {
        if (t.kind == Token::Kind::Number) return "number";
        if (t.kind == Token::Kind::End) return "end";
        return t.text;
    }

    const Token& peek() const { return i_ < end_ ? tokens_[i_] : tokens_.back(); }
    const Token& next() { return tokens_[i_ < end_ ? i_++ : tokens_.size() - 1]; }

    static NodePtr make_binary(char op, NodePtr a, NodePtr b, size_t pos) {
        auto n = std::make_shared<Node>();
        n->kind = Node::Kind::Binary;
        n->op = op;
        n->args = {std::move(a), std::move(b)};
        n->pos = pos;
        return n;
    }

    NodePtr parse_additive() {
        auto left = parse_multiplicative();
        while (peek().is_op('+') || peek().is_op('-')) {
            const auto& op = next();
            auto right = parse_multiplicative();
            left = make_binary(op.text[0], left, right, op.pos);
        }
        return left;
    }

    bool starts_implicit_operand() const {
        const auto& t = peek();
        return t.kind == Token::Kind::Ident || t.is_op('(');
    }

    NodePtr parse_multiplicative() {
        auto left = parse_unary();
        while (true) {
            const auto& t = peek();
            if (t.is_op('*') || t.is_op('/') || t.is_op('%')) {
                const auto& op = next();
                auto right = parse_unary();
                left = make_binary(op.text[0], left, right, op.pos);
            } else if (starts_implicit_operand()) {
                const size_t pos = t.pos;
                auto right = parse_power();  // no leading sign on implicit operand
                left = make_binary('*', left, right, pos);
            } else {
                break;
            }
        }
        return left;
    }

    NodePtr parse_unary() {
        if (peek().is_op('-') || peek().is_op('+')) {
            const auto& op = next();
            auto operand = parse_unary();
            if (op.text[0] == '+') return operand;
            auto n = std::make_shared<Node>();
            n->kind = Node::Kind::Unary;
            n->op = '-';
            n->args = {operand};
            n->pos = op.pos;
            return n;
        }
        return parse_power();
    }

    NodePtr parse_power() {
        auto base = parse_postfix();
        if (peek().is_op('^')) {
            const auto& op = next();
            auto exponent = parse_unary();  // right associative, allows 2^-1
            return make_binary('^', base, exponent, op.pos);
        }
        return base;
    }

    NodePtr parse_postfix() {
        auto node = parse_primary();
        while (peek().is_op('!')) {
            const auto& op = next();
            auto n = std::make_shared<Node>();
            n->kind = Node::Kind::Postfix;
            n->op = '!';
            n->args = {node};
            n->pos = op.pos;
            node = n;
        }
        return node;
    }

    NodePtr parse_primary() {
        const auto& t = peek();
        if (t.kind == Token::Kind::Number) {
            next();
            auto n = std::make_shared<Node>();
            n->kind = Node::Kind::Number;
            n->number = t.number;
            n->pos = t.pos;
            return n;
        }
        if (t.kind == Token::Kind::Ident) {
            const Token ident = next();
            if (peek().is_op('(')) {
                next();
                auto n = std::make_shared<Node>();
                n->kind = Node::Kind::Call;
                n->name = ident.text;
                n->pos = ident.pos;
                if (!peek().is_op(')')) {
                    while (true) {
                        n->args.push_back(parse_additive());
                        if (peek().is_op(',')) {
                            next();
                            continue;
                        }
                        break;
                    }
                }
                expect_close(ident.pos);
                return n;
            }
            auto n = std::make_shared<Node>();
            n->kind = Node::Kind::Name;
            n->name = ident.text;
            n->pos = ident.pos;
            return n;
        }
        if (t.is_op('(')) {
            const size_t open = t.pos;
            next();
            auto inner = parse_additive();
            expect_close(open);
            return inner;
        }
        if (t.kind == Token::Kind::End) {
            throw CalcError("incomplete expression", t.pos);
        }
        throw CalcError("unexpected '" + token_text(t) + "'", t.pos);
    }

    void expect_close(size_t open_pos) {
        if (peek().is_op(')')) {
            next();
            return;
        }
        if (peek().kind == Token::Kind::End || i_ >= end_) {
            throw CalcError("missing ')'", open_pos);
        }
        throw CalcError("expected ')'", peek().pos);
    }

    std::vector<Token> tokens_;
    size_t i_;
    size_t end_;
};

std::string trim(std::string_view s) {
    size_t a = 0;
    size_t b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return std::string(s.substr(a, b - a));
}

// ---------------------------------------------------------------- builtins

using Args = std::vector<double>;

struct Builtin {
    int min_args;
    int max_args;  // -1 = variadic
    std::function<double(const Args&, AngleMode)> fn;
};

double to_rad(double x, AngleMode m) { return m == AngleMode::Deg ? x * kPi / 180.0 : x; }
double from_rad(double x, AngleMode m) { return m == AngleMode::Deg ? x * 180.0 / kPi : x; }

double snap_zero(double v) { return std::fabs(v) < 1e-15 ? 0.0 : v; }

// 5-point central difference; h ~ eps^(1/5) balances truncation and rounding,
// the result is rounded to 10 significant digits to hide the remaining noise.
template <typename F>
double derivative(F&& f, double a) {
    const double h = 1e-3 * std::max(1.0, std::fabs(a));
    const double d = (-f(a + 2 * h) + 8 * f(a + h) - 8 * f(a - h) + f(a - 2 * h)) / (12 * h);
    if (!std::isfinite(d)) return d;
    const double scale = std::max({1.0, std::fabs(f(a)), std::fabs(a)});
    if (std::fabs(d) < 1e-9 * scale) return 0.0;
    const double mag = std::pow(10.0, std::floor(std::log10(std::fabs(d))) - 9);
    return std::round(d / mag) * mag;
}

bool is_integer(double v) { return std::isfinite(v) && std::floor(v) == v; }

double require_integer(double v, const char* fn) {
    if (!is_integer(v)) throw CalcError(std::string(fn) + ": integer expected");
    return v;
}

double factorial(double n) {
    if (std::isnan(n)) return kNaN;
    if (is_integer(n)) {
        if (n < 0) return kNaN;
        if (n > 170) return std::numeric_limits<double>::infinity();
        double r = 1;
        for (int i = 2; i <= static_cast<int>(n); ++i) r *= i;
        return r;
    }
    return std::tgamma(n + 1.0);
}

double sin_mode(double x, AngleMode m) {
    if (m == AngleMode::Deg) {
        const double r = std::fmod(x, 360.0);
        if (std::fmod(r, 180.0) == 0.0) return 0.0;
    }
    return snap_zero(std::sin(to_rad(x, m)));
}

double cos_mode(double x, AngleMode m) {
    if (m == AngleMode::Deg) {
        const double r = std::fmod(x, 360.0);
        if (std::fmod(r - 90.0, 180.0) == 0.0) return 0.0;
    }
    return snap_zero(std::cos(to_rad(x, m)));
}

double tan_mode(double x, AngleMode m) {
    if (m == AngleMode::Deg) {
        const double r = std::fmod(x, 360.0);
        if (std::fmod(r, 180.0) == 0.0) return 0.0;
        if (std::fmod(r - 90.0, 180.0) == 0.0) return kNaN;
    }
    return snap_zero(std::tan(to_rad(x, m)));
}

double gcd2(double a, double b) {
    a = std::fabs(a);
    b = std::fabs(b);
    while (b != 0) {
        const double t = std::fmod(a, b);
        a = b;
        b = t;
    }
    return a;
}

double combinations(double n, double k) {
    require_integer(n, "ncr");
    require_integer(k, "ncr");
    if (k < 0 || n < 0 || k > n) return 0;
    k = std::min(k, n - k);
    double r = 1;
    for (int i = 1; i <= static_cast<int>(k); ++i) r = r * (n - k + i) / i;
    return std::round(r);
}

double permutations(double n, double k) {
    require_integer(n, "npr");
    require_integer(k, "npr");
    if (k < 0 || n < 0 || k > n) return 0;
    double r = 1;
    for (int i = 0; i < static_cast<int>(k); ++i) r *= (n - i);
    return r;
}

const std::map<std::string, Builtin>& builtins() {
    static const std::map<std::string, Builtin> table = [] {
        std::map<std::string, Builtin> t;
        auto unary = [&t](const char* name, double (*f)(double)) {
            t[name] = {1, 1, [f](const Args& a, AngleMode) { return f(a[0]); }};
        };
        t["sin"] = {1, 1, [](const Args& a, AngleMode m) { return sin_mode(a[0], m); }};
        t["cos"] = {1, 1, [](const Args& a, AngleMode m) { return cos_mode(a[0], m); }};
        t["tan"] = {1, 1, [](const Args& a, AngleMode m) { return tan_mode(a[0], m); }};
        t["asin"] = {1, 1, [](const Args& a, AngleMode m) { return from_rad(std::asin(a[0]), m); }};
        t["acos"] = {1, 1, [](const Args& a, AngleMode m) { return from_rad(std::acos(a[0]), m); }};
        t["atan"] = {1, 1, [](const Args& a, AngleMode m) { return from_rad(std::atan(a[0]), m); }};
        t["atan2"] = {2, 2, [](const Args& a, AngleMode m) { return from_rad(std::atan2(a[0], a[1]), m); }};
        unary("sinh", std::sinh);
        unary("cosh", std::cosh);
        unary("tanh", std::tanh);
        unary("asinh", std::asinh);
        unary("acosh", std::acosh);
        unary("atanh", std::atanh);
        unary("sqrt", std::sqrt);
        unary("cbrt", std::cbrt);
        unary("abs", std::fabs);
        unary("exp", std::exp);
        unary("ln", std::log);
        unary("log2", std::log2);
        unary("log10", std::log10);
        unary("floor", std::floor);
        unary("ceil", std::ceil);
        unary("trunc", std::trunc);
        unary("gamma", std::tgamma);
        t["log"] = {1, 2, [](const Args& a, AngleMode) {
                        // log(x) = log10(x); log(b, x) = log base b of x
                        return a.size() == 1 ? std::log10(a[0]) : std::log(a[1]) / std::log(a[0]);
                    }};
        t["root"] = {2, 2, [](const Args& a, AngleMode) {
                         // n-th root, real for odd n with negative x
                         const double x = a[0];
                         const double n = a[1];
                         if (x < 0 && is_integer(n) && std::fmod(std::fabs(n), 2.0) == 1.0) {
                             return -std::pow(-x, 1.0 / n);
                         }
                         return std::pow(x, 1.0 / n);
                     }};
        t["round"] = {1, 2, [](const Args& a, AngleMode) {
                          if (a.size() == 1) return std::round(a[0]);
                          const double p = std::pow(10.0, require_integer(a[1], "round"));
                          return std::round(a[0] * p) / p;
                      }};
        t["sign"] = {1, 1, [](const Args& a, AngleMode) {
                         return static_cast<double>((a[0] > 0) - (a[0] < 0));
                     }};
        t["mod"] = {2, 2, [](const Args& a, AngleMode) {
                        // mathematical modulo: result has the sign of the divisor
                        const double r = std::fmod(a[0], a[1]);
                        return (r != 0 && ((r < 0) != (a[1] < 0))) ? r + a[1] : r;
                    }};
        t["min"] = {1, -1, [](const Args& a, AngleMode) { return *std::min_element(a.begin(), a.end()); }};
        t["max"] = {1, -1, [](const Args& a, AngleMode) { return *std::max_element(a.begin(), a.end()); }};
        t["hypot"] = {2, 2, [](const Args& a, AngleMode) { return std::hypot(a[0], a[1]); }};
        t["gcd"] = {2, 2, [](const Args& a, AngleMode) {
                        return gcd2(require_integer(a[0], "gcd"), require_integer(a[1], "gcd"));
                    }};
        t["lcm"] = {2, 2, [](const Args& a, AngleMode) {
                        const double x = require_integer(a[0], "lcm");
                        const double y = require_integer(a[1], "lcm");
                        if (x == 0 || y == 0) return 0.0;
                        return std::fabs(x / gcd2(x, y) * y);
                    }};
        t["ncr"] = {2, 2, [](const Args& a, AngleMode) { return combinations(a[0], a[1]); }};
        t["npr"] = {2, 2, [](const Args& a, AngleMode) { return permutations(a[0], a[1]); }};
        t["fact"] = {1, 1, [](const Args& a, AngleMode) { return factorial(a[0]); }};
        // explicit unit conversions, independent of the angle mode
        t["torad"] = {1, 1, [](const Args& a, AngleMode) { return a[0] * kPi / 180.0; }};
        t["todeg"] = {1, 1, [](const Args& a, AngleMode) { return a[0] * 180.0 / kPi; }};
        return t;
    }();
    return table;
}

const std::map<std::string, double>& constants() {
    static const std::map<std::string, double> table = {
        {"pi", kPi},
        {"e", 2.71828182845904523536},
        {"tau", 2 * kPi},
        {"phi", 1.61803398874989484820},
        {"inf", std::numeric_limits<double>::infinity()},
    };
    return table;
}

// Special forms take an expression in `x` as their first argument and evaluate
// it lazily: der(f, a), integ(f, a, b), solve(f, guess) / solve(f, a, b), sum(f, a, b).
bool is_special(const std::string& name) {
    return name == "der" || name == "integ" || name == "solve" || name == "sum";
}

}  // namespace

// ---------------------------------------------------------------- public API

double numeric_derivative(const std::function<double(double)>& f, double a) { return derivative(f, a); }

NodePtr parse_expression(std::string_view source) {
    auto tokens = tokenize(source);
    size_t end = tokens.size() - 1;
    if (end == 0) throw CalcError("empty expression", 0);
    for (size_t i = 0; i < end; ++i) {
        if (tokens[i].is_op('=')) throw CalcError("unexpected '='", tokens[i].pos);
    }
    Parser p(std::move(tokens), 0, end);
    return p.parse_all();
}

Statement parse_statement(std::string_view source) {
    auto tokens = tokenize(source);
    size_t end = tokens.size() - 1;  // index of End token
    if (end == 0) throw CalcError("empty expression", 0);

    // tolerate a single trailing '=' ("2+2=")
    if (tokens[end - 1].is_op('=')) {
        --end;
        if (end == 0) throw CalcError("empty expression", 0);
    }

    Statement st;
    size_t body_begin = 0;

    if (tokens[0].kind == Token::Kind::Ident && end > 1 && tokens[1].is_op('=')) {
        st.kind = Statement::Kind::AssignVariable;
        st.name = tokens[0].text;
        body_begin = 2;
    } else if (tokens[0].kind == Token::Kind::Ident && end > 2 && tokens[1].is_op('(')) {
        // f(a, b) = ...
        size_t j = 2;
        std::vector<std::string> params;
        bool ok = true;
        while (j < end && tokens[j].kind == Token::Kind::Ident) {
            params.push_back(tokens[j].text);
            ++j;
            if (j < end && tokens[j].is_op(',')) {
                ++j;
                continue;
            }
            break;
        }
        ok = ok && !params.empty() && j + 1 < end && tokens[j].is_op(')') && tokens[j + 1].is_op('=');
        if (ok) {
            st.kind = Statement::Kind::DefineFunction;
            st.name = tokens[0].text;
            st.params = std::move(params);
            body_begin = j + 2;
        }
    }

    if (st.kind != Statement::Kind::Expression) {
        if (Env::is_reserved(st.name)) {
            throw CalcError("'" + st.name + "' is reserved", tokens[0].pos);
        }
        for (size_t a = 0; a < st.params.size(); ++a) {
            for (size_t b = a + 1; b < st.params.size(); ++b) {
                if (st.params[a] == st.params[b]) throw CalcError("duplicate parameter", tokens[0].pos);
            }
        }
        if (body_begin >= end) {
            throw CalcError("missing value after '='", tokens[body_begin - 1].pos);
        }
    }

    for (size_t i = body_begin; i < end; ++i) {
        if (tokens[i].is_op('=')) throw CalcError("unexpected '='", tokens[i].pos);
    }

    const size_t src_begin = tokens[body_begin].pos;
    const size_t src_end = tokens[end - 1].end;
    st.body_source = trim(source.substr(src_begin, src_end - src_begin));

    Parser p(std::move(tokens), body_begin, end);
    st.body = p.parse_all();
    return st;
}

bool Env::is_builtin_function(const std::string& name) {
    return builtins().count(name) > 0 || is_special(name);
}

bool Env::is_reserved(const std::string& name) {
    return is_builtin_function(name) || constants().count(name) > 0 || name == "ans";
}

std::vector<std::string> Env::builtin_names() {
    std::vector<std::string> names;
    for (const auto& [name, _] : builtins()) names.push_back(name);
    names.push_back("der");
    names.push_back("integ");
    names.push_back("solve");
    names.push_back("sum");
    std::sort(names.begin(), names.end());
    return names;
}

double Env::lookup(const Node& node, const Locals* locals) const {
    if (locals) {
        auto it = locals->find(node.name);
        if (it != locals->end()) return it->second;
    }
    if (auto it = vars.find(node.name); it != vars.end()) return it->second;
    if (auto it = constants().find(node.name); it != constants().end()) return it->second;
    if (funcs.count(node.name) || is_builtin_function(node.name)) {
        throw CalcError("'" + node.name + "' needs arguments: " + node.name + "(...)", node.pos);
    }
    throw CalcError("unknown name '" + node.name + "'", node.pos);
}

double Env::eval(const NodePtr& node, const Locals* locals, int depth) const {
    if (depth > kMaxDepth) throw CalcError("recursion too deep", node->pos);
    switch (node->kind) {
        case Node::Kind::Number:
            return node->number;
        case Node::Kind::Name:
            return lookup(*node, locals);
        case Node::Kind::Unary:
            return -eval(node->args[0], locals, depth + 1);
        case Node::Kind::Postfix:
            return factorial(eval(node->args[0], locals, depth + 1));
        case Node::Kind::Binary: {
            const double a = eval(node->args[0], locals, depth + 1);
            const double b = eval(node->args[1], locals, depth + 1);
            switch (node->op) {
                case '+': return a + b;
                case '-': return a - b;
                case '*': return a * b;
                case '/':
                    if (b == 0) throw CalcError("division by zero", node->pos);
                    return a / b;
                case '%':
                    if (b == 0) throw CalcError("division by zero", node->pos);
                    return std::fmod(a, b);
                case '^':
                    return std::pow(a, b);
                default:
                    throw CalcError("bad operator", node->pos);
            }
        }
        case Node::Kind::Call:
            return call(*node, locals, depth);
    }
    return kNaN;
}

double Env::call(const Node& node, const Locals* locals, int depth) const {
    if (is_special(node.name)) return call_special(node, locals, depth);

    if (auto uf = funcs.find(node.name); uf != funcs.end()) {
        const auto& f = uf->second;
        if (node.args.size() != f.params.size()) {
            throw CalcError(node.name + ": expects " + std::to_string(f.params.size()) + " argument(s)",
                            node.pos);
        }
        Locals inner;
        for (size_t i = 0; i < f.params.size(); ++i) {
            inner[f.params[i]] = eval(node.args[i], locals, depth + 1);
        }
        return eval(f.body, &inner, depth + 1);
    }

    if (auto bi = builtins().find(node.name); bi != builtins().end()) {
        const auto& b = bi->second;
        const int n = static_cast<int>(node.args.size());
        if (n < b.min_args || (b.max_args >= 0 && n > b.max_args)) {
            std::string expected = std::to_string(b.min_args);
            if (b.max_args < 0) expected += "+";
            else if (b.max_args != b.min_args) expected += "-" + std::to_string(b.max_args);
            throw CalcError(node.name + ": expects " + expected + " argument(s)", node.pos);
        }
        Args args;
        args.reserve(node.args.size());
        for (const auto& a : node.args) args.push_back(eval(a, locals, depth + 1));
        try {
            return b.fn(args, angle);
        } catch (const CalcError& e) {
            throw CalcError(e.what(), node.pos);
        }
    }

    // `k(2)` where k is a plain value: implicit multiplication
    if (node.args.size() == 1) {
        Node name_node;
        name_node.kind = Node::Kind::Name;
        name_node.name = node.name;
        name_node.pos = node.pos;
        double value;
        try {
            value = lookup(name_node, locals);
        } catch (const CalcError&) {
            throw CalcError("unknown function '" + node.name + "'", node.pos);
        }
        return value * eval(node.args[0], locals, depth + 1);
    }
    throw CalcError("unknown function '" + node.name + "'", node.pos);
}

double Env::call_special(const Node& node, const Locals* locals, int depth) const {
    const auto& name = node.name;
    const size_t n = node.args.size();
    auto arity_error = [&](const char* usage) { throw CalcError(std::string("usage: ") + usage, node.pos); };

    Locals inner = locals ? *locals : Locals{};
    auto f = [&](double x) {
        inner["x"] = x;
        return eval(node.args[0], &inner, depth + 1);
    };
    auto arg = [&](size_t i) { return eval(node.args[i], locals, depth + 1); };

    if (name == "der") {
        if (n != 2) arity_error("der(f(x), at)");
        const double a = arg(1);
        return derivative(f, a);
    }

    if (name == "integ") {
        if (n != 3) arity_error("integ(f(x), a, b)");
        const double a = arg(1);
        const double b = arg(2);
        if (!std::isfinite(a) || !std::isfinite(b)) throw CalcError("integ: finite bounds expected", node.pos);
        // adaptive Simpson
        std::function<double(double, double, double, double, double, double, int)> rec =
            [&](double lo, double hi, double flo, double fmid, double fhi, double whole, int level) {
                const double mid = (lo + hi) / 2;
                const double lm = (lo + mid) / 2;
                const double rm = (mid + hi) / 2;
                const double flm = f(lm);
                const double frm = f(rm);
                const double left = (mid - lo) / 6 * (flo + 4 * flm + fmid);
                const double right = (hi - mid) / 6 * (fmid + 4 * frm + fhi);
                const double diff = left + right - whole;
                if (level <= 0 || std::fabs(diff) <= 1e-10 * std::max(1.0, std::fabs(left + right))) {
                    return left + right + diff / 15;
                }
                return rec(lo, mid, flo, flm, fmid, left, level - 1) +
                       rec(mid, hi, fmid, frm, fhi, right, level - 1);
            };
        if (a == b) return 0.0;
        const double fa = f(a);
        const double fb = f(b);
        const double fm = f((a + b) / 2);
        const double whole = (b - a) / 6 * (fa + 4 * fm + fb);
        const double r = rec(a, b, fa, fm, fb, whole, 18);
        if (!std::isfinite(r)) throw CalcError("integ: does not converge", node.pos);
        return snap_zero(r);
    }

    if (name == "solve") {
        if (n == 3) {
            double lo = arg(1);
            double hi = arg(2);
            double flo = f(lo);
            double fhi = f(hi);
            if (flo == 0) return lo;
            if (fhi == 0) return hi;
            if ((flo < 0) == (fhi < 0)) throw CalcError("solve: no sign change in [a, b]", node.pos);
            for (int i = 0; i < 200 && hi - lo > 1e-15 * std::max(1.0, std::fabs(lo)); ++i) {
                const double mid = (lo + hi) / 2;
                const double fm = f(mid);
                if (fm == 0) return mid;
                if ((fm < 0) == (flo < 0)) {
                    lo = mid;
                    flo = fm;
                } else {
                    hi = mid;
                }
            }
            return (lo + hi) / 2;
        }
        if (n != 2) arity_error("solve(f(x), guess) or solve(f(x), a, b)");
        // secant method from the guess
        double x0 = arg(1);
        double x1 = x0 + 1e-4 * std::max(1.0, std::fabs(x0));
        double f0 = f(x0);
        double f1 = f(x1);
        for (int i = 0; i < 100; ++i) {
            if (!std::isfinite(f1)) break;
            if (std::fabs(f1) < 1e-14) return snap_zero(x1);
            const double denom = f1 - f0;
            if (denom == 0) break;
            const double x2 = x1 - f1 * (x1 - x0) / denom;
            x0 = x1;
            f0 = f1;
            x1 = x2;
            f1 = f(x1);
            if (std::fabs(x1 - x0) < 1e-13 * std::max(1.0, std::fabs(x1))) {
                if (std::fabs(f1) < 1e-8 * std::max(1.0, std::fabs(f0)) || std::fabs(f1) < 1e-9) {
                    return snap_zero(x1);
                }
            }
        }
        throw CalcError("solve: no root found near guess", node.pos);
    }

    if (name == "sum") {
        if (n != 3) arity_error("sum(f(x), from, to)");
        const double a = arg(1);
        const double b = arg(2);
        if (!is_integer(a) || !is_integer(b)) throw CalcError("sum: integer bounds expected", node.pos);
        if (b - a > 1e6) throw CalcError("sum: too many terms", node.pos);
        double s = 0;
        for (double x = a; x <= b; x += 1) s += f(x);
        return s;
    }
    throw CalcError("unknown function '" + name + "'", node.pos);
}

}  // namespace calc
