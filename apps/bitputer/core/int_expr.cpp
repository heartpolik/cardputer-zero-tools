#include "int_expr.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <functional>

namespace bits {
namespace {

bool is_ident_start(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool is_ident_char(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }
bool is_hex_word(const std::string& w) {
    if (w.empty()) return false;
    for (char c : w) {
        if (!std::isxdigit(static_cast<unsigned char>(c)) && c != '_') return false;
    }
    return true;
}

int digit_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 99;
}

uint64_t field_mask(int n) { return n >= 64 ? ~uint64_t{0} : (uint64_t{1} << n) - 1; }

const std::vector<FunctionInfo> kFunctions = {
    {"popcnt", "x", "number of 1 bits"},
    {"clz", "x", "leading zeros (in the word)"},
    {"ctz", "x", "trailing zeros"},
    {"parity", "x", "1 if odd number of 1 bits"},
    {"bswap", "x", "reverse byte order (BE <-> LE)"},
    {"bswap16", "x", "swap bytes in each 16-bit word"},
    {"wswap", "x", "reverse 16-bit words (Modbus)"},
    {"rev", "x", "reverse bit order"},
    {"bit", "n", "1 << n"},
    {"mask", "n | hi,lo", "n low bits / bits hi..lo set"},
    {"field", "x,hi,lo", "extract bits hi..lo"},
    {"set", "x,n", "set bit n"},
    {"clr", "x,n", "clear bit n"},
    {"tgl", "x,n", "toggle bit n"},
    {"rol", "x,n", "rotate left in the word"},
    {"ror", "x,n", "rotate right in the word"},
    {"sext", "x,bits", "sign-extend from bits"},
    {"zext", "x,bits", "keep the low bits"},
    {"u8", "x", "low byte"},
    {"u16", "x", "low 16 bits"},
    {"u32", "x", "low 32 bits"},
    {"i8", "x", "sign-extend from 8 bits"},
    {"i16", "x", "sign-extend from 16 bits"},
    {"i32", "x", "sign-extend from 32 bits"},
    {"min", "a,b", "smaller (signed in S mode)"},
    {"max", "a,b", "larger"},
    {"abs", "x", "absolute value (signed)"},
    {"align", "x,a", "round x up to a multiple of a"},
    {"isqrt", "x", "integer square root"},
    {"log2", "x", "floor(log2 x)"},
    {"gcd", "a,b", "greatest common divisor"},
    {"f32", "1.5", "IEEE-754 single bits"},
    {"f64", "1.5", "IEEE-754 double bits"},
};

bool is_function(const std::string& name) {
    for (const auto& f : kFunctions) {
        if (name == f.name) return true;
    }
    return false;
}

class Parser {
public:
    Parser(const std::string& src, const Env& env) : s_(src), env_(env), w_(env.word) {}

    uint64_t parse_all(size_t start) {
        pos_ = start;
        const uint64_t v = parse_or();
        skip_ws();
        if (pos_ < s_.size()) {
            if (s_[pos_] == ')') throw ExprError("unmatched ')'", pos_);
            throw ExprError(std::string("unexpected '") + s_[pos_] + "'", pos_);
        }
        return v;
    }

private:
    void skip_ws() {
        while (pos_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[pos_]))) ++pos_;
    }
    bool peek(const char* op) {
        skip_ws();
        return s_.compare(pos_, std::strlen(op), op) == 0;
    }
    char peek_char() {
        skip_ws();
        return pos_ < s_.size() ? s_[pos_] : '\0';
    }
    void expect(char c) {
        if (peek_char() != c) {
            if (pos_ >= s_.size()) throw ExprError(std::string("missing '") + c + "'", pos_);
            throw ExprError(std::string("expected '") + c + "'", pos_);
        }
        ++pos_;
    }

    uint64_t wrap(uint64_t v) const { return w_.wrap(v); }

    uint64_t parse_or() {
        uint64_t v = parse_xor();
        while (peek_char() == '|') {
            ++pos_;
            v = wrap(v | parse_xor());
        }
        return v;
    }
    uint64_t parse_xor() {
        uint64_t v = parse_and();
        while (peek_char() == '^') {
            ++pos_;
            v = wrap(v ^ parse_and());
        }
        return v;
    }
    uint64_t parse_and() {
        uint64_t v = parse_shift();
        while (peek_char() == '&') {
            ++pos_;
            v = wrap(v & parse_shift());
        }
        return v;
    }
    uint64_t parse_shift() {
        uint64_t v = parse_add();
        for (;;) {
            if (peek(">>>")) {
                pos_ += 3;
                v = shift_right(v, parse_add(), false);
            } else if (peek(">>")) {
                pos_ += 2;
                v = shift_right(v, parse_add(), w_.is_signed);
            } else if (peek("<<")) {
                pos_ += 2;
                const uint64_t n = parse_add();
                v = n >= static_cast<uint64_t>(w_.width) ? 0 : wrap(v << n);
            } else {
                return v;
            }
        }
    }
    uint64_t shift_right(uint64_t v, uint64_t n, bool arithmetic) const {
        const bool neg = arithmetic && w_.negative(v);
        if (n >= static_cast<uint64_t>(w_.width)) return neg ? w_.mask() : 0;
        if (!neg) return wrap(v) >> n;
        return wrap(static_cast<uint64_t>(w_.to_signed(v) >> n));
    }
    uint64_t parse_add() {
        uint64_t v = parse_mul();
        for (;;) {
            const char c = peek_char();
            if (c == '+') {
                ++pos_;
                v = wrap(v + parse_mul());
            } else if (c == '-') {
                ++pos_;
                v = wrap(v - parse_mul());
            } else {
                return v;
            }
        }
    }
    uint64_t parse_mul() {
        uint64_t v = parse_unary();
        for (;;) {
            const char c = peek_char();
            if (c == '*' && !peek("**")) {
                ++pos_;
                v = wrap(v * parse_unary());
            } else if (c == '/' || c == '%') {
                const size_t op = pos_++;
                const uint64_t r = parse_unary();
                v = divide(v, r, c == '%', op);
            } else {
                return v;
            }
        }
    }
    uint64_t divide(uint64_t a, uint64_t b, bool mod, size_t op) const {
        if (wrap(b) == 0) throw ExprError("division by zero", op);
        if (!w_.is_signed) return mod ? wrap(a) % wrap(b) : wrap(a) / wrap(b);
        const int64_t x = w_.to_signed(a), y = w_.to_signed(b);
        if (y == -1) return mod ? 0 : wrap(0 - wrap(a));  // avoids INT64_MIN / -1
        return wrap(static_cast<uint64_t>(mod ? x % y : x / y));
    }
    uint64_t parse_unary() {
        const char c = peek_char();
        if (c == '-') {
            ++pos_;
            return wrap(0 - parse_unary());
        }
        if (c == '+') {
            ++pos_;
            return parse_unary();
        }
        if (c == '~') {
            ++pos_;
            return wrap(~parse_unary());
        }
        return parse_power();
    }
    uint64_t parse_power() {
        const uint64_t base = parse_primary();
        if (peek("**")) {
            const size_t op = pos_;
            pos_ += 2;
            const uint64_t e = parse_unary();  // right-associative
            if (w_.is_signed && w_.negative(e)) throw ExprError("negative exponent", op);
            uint64_t result = 1, b = base, n = wrap(e);
            while (n) {
                if (n & 1) result = wrap(result * b);
                b = wrap(b * b);
                n >>= 1;
            }
            return result;
        }
        return base;
    }

    uint64_t parse_primary() {
        skip_ws();
        if (pos_ >= s_.size()) throw ExprError("missing value", pos_);
        const char c = s_[pos_];
        if (c == '(') {
            ++pos_;
            const uint64_t v = parse_or();
            expect(')');
            return v;
        }
        if (c == '\'') return parse_chars();
        if (std::isdigit(static_cast<unsigned char>(c))) return parse_number();
        if (is_ident_start(c)) {
            const size_t start = pos_;
            size_t end = pos_;
            while (end < s_.size() && is_ident_char(s_[end])) ++end;
            const std::string name = s_.substr(start, end - start);
            size_t after = end;
            while (after < s_.size() && s_[after] == ' ') ++after;
            const bool call = after < s_.size() && s_[after] == '(';
            if (!call && env_.input == Base::Hex && is_hex_word(name)) return parse_number();
            pos_ = end;
            if (call) {
                pos_ = after + 1;
                return call_function(name, start);
            }
            if (is_function(name)) throw ExprError(name + " needs (...)", start);
            const auto it = env_.vars.find(name);
            if (it != env_.vars.end()) return wrap(it->second);
            if (name == "ans") return 0;
            throw ExprError("unknown name '" + name + "'", start);
        }
        throw ExprError(std::string("unexpected '") + c + "'", pos_);
    }

    uint64_t parse_chars() {
        const size_t start = pos_++;
        uint64_t v = 0;
        int count = 0;
        while (pos_ < s_.size() && s_[pos_] != '\'') {
            char ch = s_[pos_++];
            if (ch == '\\' && pos_ < s_.size()) {
                const char e = s_[pos_++];
                ch = e == 'n' ? '\n' : e == 'r' ? '\r' : e == 't' ? '\t' : e == '0' ? '\0' : e;
            }
            if (++count > 8) throw ExprError("more than 8 chars", start);
            v = (v << 8) | static_cast<unsigned char>(ch);
        }
        if (pos_ >= s_.size()) throw ExprError("missing '", start);
        ++pos_;
        if (count == 0) throw ExprError("empty char literal", start);
        return wrap(v);
    }

    uint64_t parse_number() {
        const size_t start = pos_;
        size_t end = pos_;
        while (end < s_.size() && is_ident_char(s_[end])) ++end;
        std::string word = s_.substr(start, end - start);
        pos_ = end;

        unsigned base = static_cast<unsigned>(env_.input);
        size_t digits_at = 0;
        if (word.size() > 2 && word[0] == '0') {
            const char p = static_cast<char>(std::tolower(static_cast<unsigned char>(word[1])));
            if (p == 'x') base = 16, digits_at = 2;
            else if (p == 'o') base = 8, digits_at = 2;
            else if (p == 'n') base = 10, digits_at = 2;
            else if (p == 'b' && env_.input != Base::Hex) base = 2, digits_at = 2;
        }
        int shift = 0;
        if (word.size() > digits_at + 1) {
            const char suffix = static_cast<char>(std::toupper(static_cast<unsigned char>(word.back())));
            const int s = suffix == 'K' ? 10 : suffix == 'M' ? 20 : suffix == 'G' ? 30 : 0;
            if (s && digit_value(word.back()) >= static_cast<int>(base)) {
                shift = s;
                word.pop_back();
            }
        }

        uint64_t v = 0;
        bool any = false;
        for (size_t i = digits_at; i < word.size(); ++i) {
            if (word[i] == '_') continue;
            const int d = digit_value(word[i]);
            if (d >= static_cast<int>(base)) {
                throw ExprError(std::string("'") + word[i] + "' is not a " + base_label(base) + " digit", start + i);
            }
            if (v > (~uint64_t{0} - static_cast<uint64_t>(d)) / base) throw ExprError("number too large", start);
            v = v * base + static_cast<uint64_t>(d);
            any = true;
        }
        if (!any) throw ExprError("missing digits", start);
        if (shift) {
            if (v > (~uint64_t{0} >> shift)) throw ExprError("number too large", start);
            v <<= shift;
        }
        return wrap(v);
    }
    static const char* base_label(unsigned base) {
        return base == 16 ? "hex" : base == 8 ? "octal" : base == 2 ? "binary" : "decimal";
    }

    std::vector<uint64_t> parse_args() {
        std::vector<uint64_t> args;
        if (peek_char() == ')') {
            ++pos_;
            return args;
        }
        for (;;) {
            args.push_back(parse_or());
            const char c = peek_char();
            if (c == ',') {
                ++pos_;
                continue;
            }
            expect(')');
            return args;
        }
    }

    uint64_t parse_float_bits(bool single, size_t name_pos) {
        skip_ws();
        const size_t start = pos_;
        const size_t close = s_.find(')', pos_);
        if (close == std::string::npos) throw ExprError("missing ')'", s_.size());
        const std::string text = s_.substr(start, close - start);
        char* end = nullptr;
        const double d = std::strtod(text.c_str(), &end);
        size_t used = static_cast<size_t>(end - text.c_str());
        while (used < text.size() && text[used] == ' ') ++used;
        if (text.empty() || used != text.size()) throw ExprError("expected a number like 1.5e-3", start);
        pos_ = close + 1;
        (void)name_pos;
        if (single) {
            const float f = static_cast<float>(d);
            uint32_t b;
            std::memcpy(&b, &f, sizeof b);
            return wrap(b);
        }
        uint64_t b;
        std::memcpy(&b, &d, sizeof b);
        return wrap(b);
    }

    uint64_t call_function(const std::string& name, size_t at) {
        if (name == "f32" || name == "f64") return parse_float_bits(name == "f32", at);
        if (!is_function(name)) throw ExprError("unknown function '" + name + "'", at);
        const auto a = parse_args();
        auto need = [&](size_t n) {
            if (a.size() != n) {
                throw ExprError(name + " takes " + std::to_string(n) + " argument" + (n == 1 ? "" : "s"), at);
            }
        };
        auto bit_index = [&](uint64_t n) {
            if (n >= static_cast<uint64_t>(w_.width)) {
                throw ExprError("bit " + std::to_string(n) + " outside the " + std::to_string(w_.width) + "-bit word", at);
            }
            return static_cast<int>(n);
        };
        auto width_arg = [&](uint64_t n) {
            if (n < 1 || n > 64) throw ExprError("bit count must be 1..64", at);
            return static_cast<int>(n);
        };
        const int W = w_.width;
        const uint64_t x = a.empty() ? 0 : wrap(a[0]);

        if (name == "popcnt") return need(1), static_cast<uint64_t>(__builtin_popcountll(x));
        if (name == "parity") return need(1), static_cast<uint64_t>(__builtin_popcountll(x) & 1);
        if (name == "clz") return need(1), x == 0 ? W : static_cast<uint64_t>(__builtin_clzll(x) - (64 - W));
        if (name == "ctz") return need(1), x == 0 ? W : static_cast<uint64_t>(__builtin_ctzll(x));
        if (name == "bswap16") return need(1), reorder(x, W, ByteOrder::ByteSwap16);
        if (name == "wswap") return need(1), reorder(x, W, ByteOrder::WordSwap16);
        if (name == "bswap") {
            need(1);
            uint64_t r = 0;
            for (int i = 0; i < W / 8; ++i) r = (r << 8) | ((x >> (i * 8)) & 0xFF);
            return r;
        }
        if (name == "rev") {
            need(1);
            uint64_t r = 0;
            for (int i = 0; i < W; ++i) r = (r << 1) | ((x >> i) & 1);
            return r;
        }
        if (name == "bit") return need(1), wrap(uint64_t{1} << bit_index(x));
        if (name == "mask") {
            if (a.size() == 1) return wrap(field_mask(width_arg(x)));
            need(2);
            const int hi = bit_index(a[0]), lo = bit_index(a[1]);
            if (hi < lo) throw ExprError("mask(hi, lo) needs hi >= lo", at);
            return wrap(field_mask(hi - lo + 1) << lo);
        }
        if (name == "field") {
            need(3);
            const int hi = bit_index(a[1]), lo = bit_index(a[2]);
            if (hi < lo) throw ExprError("field(x, hi, lo) needs hi >= lo", at);
            return (x >> lo) & field_mask(hi - lo + 1);
        }
        if (name == "set") return need(2), wrap(x | (uint64_t{1} << bit_index(a[1])));
        if (name == "clr") return need(2), wrap(x & ~(uint64_t{1} << bit_index(a[1])));
        if (name == "tgl") return need(2), wrap(x ^ (uint64_t{1} << bit_index(a[1])));
        if (name == "rol" || name == "ror") {
            need(2);
            int n = static_cast<int>(wrap(a[1]) % static_cast<uint64_t>(W));
            if (name == "ror") n = (W - n) % W;
            if (n == 0) return x;
            return wrap((x << n) | (x >> (W - n)));
        }
        if (name == "sext" || name == "i8" || name == "i16" || name == "i32") {
            int b;
            if (name == "sext") {
                need(2);
                b = width_arg(a[1]);
            } else {
                need(1);
                b = name == "i8" ? 8 : name == "i16" ? 16 : 32;
            }
            uint64_t v = x & field_mask(b);
            if (b < 64 && ((v >> (b - 1)) & 1)) v |= ~field_mask(b);
            return wrap(v);
        }
        if (name == "zext") return need(2), x & field_mask(width_arg(a[1]));
        if (name == "u8") return need(1), x & 0xFF;
        if (name == "u16") return need(1), x & 0xFFFF;
        if (name == "u32") return need(1), x & 0xFFFFFFFFULL;
        if (name == "min") return need(2), w_.less(a[0], a[1]) ? wrap(a[0]) : wrap(a[1]);
        if (name == "max") return need(2), w_.less(a[0], a[1]) ? wrap(a[1]) : wrap(a[0]);
        if (name == "abs") return need(1), (w_.is_signed && w_.negative(x)) ? wrap(0 - x) : x;
        if (name == "align") {
            need(2);
            const uint64_t al = wrap(a[1]);
            if (al == 0) throw ExprError("align to 0", at);
            return wrap((x + al - 1) / al * al);
        }
        if (name == "isqrt") {
            need(1);
            uint64_t r = static_cast<uint64_t>(std::sqrt(static_cast<long double>(x)));
            while (r > 0 && (r > x / r)) --r;  // r*r > x without overflow
            while ((r + 1) <= x / (r + 1)) ++r;
            return r;
        }
        if (name == "log2") {
            need(1);
            if (x == 0) throw ExprError("log2(0)", at);
            return static_cast<uint64_t>(63 - __builtin_clzll(x));
        }
        if (name == "gcd") {
            need(2);
            uint64_t p = wrap(a[0]), q = wrap(a[1]);
            while (q) {
                const uint64_t t = p % q;
                p = q;
                q = t;
            }
            return p;
        }
        throw ExprError("unknown function '" + name + "'", at);
    }

    const std::string& s_;
    const Env& env_;
    const Word& w_;
    size_t pos_{0};
};

// "name = expr" -> (name, offset of expr); otherwise (empty, 0)
std::pair<std::string, size_t> split_assignment(const std::string& src, const Env& env) {
    size_t i = 0;
    while (i < src.size() && src[i] == ' ') ++i;
    if (i >= src.size() || !is_ident_start(src[i])) return {"", 0};
    const size_t start = i;
    while (i < src.size() && is_ident_char(src[i])) ++i;
    const std::string name = src.substr(start, i - start);
    while (i < src.size() && src[i] == ' ') ++i;
    if (i >= src.size() || src[i] != '=') return {"", 0};
    if (env.input == Base::Hex && is_hex_word(name)) {
        throw ExprError("'" + name + "' is a hex number in HEX mode", start);
    }
    if (is_function(name)) throw ExprError("'" + name + "' is a function", start);
    return {name, i + 1};
}

}  // namespace

EvalResult evaluate(const std::string& src, const Env& env) {
    EvalResult r;
    try {
        const auto [name, expr_at] = split_assignment(src, env);
        Parser p(src, env);
        r.value = p.parse_all(expr_at);
        r.assigned = name;
        r.ok = true;
    } catch (const ExprError& e) {
        r.error = e.what();
        r.error_pos = e.pos();
    }
    return r;
}

EvalResult submit(const std::string& src, Env& env) {
    EvalResult r = evaluate(src, env);
    if (!r.ok) return r;
    if (!r.assigned.empty()) env.vars[r.assigned] = r.value;
    env.vars["ans"] = r.value;
    return r;
}

const std::vector<FunctionInfo>& functions() { return kFunctions; }

}  // namespace bits
