#include "si.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ohm {
namespace {

int prefix_exponent(char c) {
    switch (c) {
        case 'p': return -12;
        case 'n': return -9;
        case 'u': return -6;
        case 'm': return -3;
        case 'R':
        case 'r': return 0;
        case 'k':
        case 'K': return 3;
        case 'M': return 6;
        case 'G': return 9;
        default: return 99;
    }
}

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && s[a] == ' ') ++a;
    while (b > a && s[b - 1] == ' ') --b;
    return s.substr(a, b - a);
}

bool is_unit_tail(const std::string& t) {
    static const char* units[] = {"", "ohm", "ohms", "Ohm", "\xCE\xA9", "V", "v", "A", "W", "F", "H", "Hz", "hz",
                                  "s", "%", "Ah", "Wh", "h"};
    for (const char* u : units) {
        if (t == u) return true;
    }
    return false;
}

std::string trim_zeros(std::string s) {
    if (s.find('.') == std::string::npos) return s;
    while (!s.empty() && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    return s;
}

}  // namespace

std::optional<double> parse_si(const std::string& input) {
    std::string s;
    for (char c : input) {
        if (c != ' ') s += c;  // "4.7 uF"
    }
    // "µ" (UTF-8 C2 B5) -> u
    for (size_t p; (p = s.find("\xC2\xB5")) != std::string::npos;) s.replace(p, 2, "u");
    if (s.empty()) return std::nullopt;

    // mantissa digits, an optional '.', or a prefix letter used as the decimal point
    std::string number;
    int exponent = 0;
    bool have_prefix = false, have_digit = false, have_point = false;
    size_t i = 0;
    if (s[i] == '-' || s[i] == '+') number += s[i++];
    for (; i < s.size(); ++i) {
        const char c = s[i];
        if (std::isdigit(static_cast<unsigned char>(c))) {
            number += c;
            have_digit = true;
        } else if (c == '.' && !have_point && !have_prefix) {
            number += c;
            have_point = true;
        } else if ((c == 'e' || c == 'E') && have_digit && !have_prefix && i + 1 < s.size() &&
                   (std::isdigit(static_cast<unsigned char>(s[i + 1])) || s[i + 1] == '-' || s[i + 1] == '+')) {
            char* end = nullptr;
            const long e = std::strtol(s.c_str() + i + 1, &end, 10);
            exponent += static_cast<int>(e);
            i = static_cast<size_t>(end - s.c_str()) - 1;
        } else if (!have_prefix && prefix_exponent(c) != 99) {
            // a prefix letter followed by digits is the decimal point ("4k7"); otherwise it ends the number
            const bool digits_follow = i + 1 < s.size() && std::isdigit(static_cast<unsigned char>(s[i + 1]));
            if (have_point && digits_follow) return std::nullopt;  // "4.7k7"
            exponent += prefix_exponent(c);
            have_prefix = true;
            if (digits_follow) {
                number += '.';
                have_point = true;
            } else {
                ++i;
                break;
            }
        } else {
            break;
        }
    }
    if (!have_digit) return std::nullopt;
    if (!is_unit_tail(trim(s.substr(i)))) return std::nullopt;
    char* end = nullptr;
    const double v = std::strtod(number.c_str(), &end);
    if (end == number.c_str()) return std::nullopt;
    return v * std::pow(10.0, exponent);
}

std::string format_plain(double value, int digits) {
    if (value == 0) return "0";
    char buf[48];
    const int mag = static_cast<int>(std::floor(std::log10(std::fabs(value))));
    int decimals = digits - 1 - mag;
    if (decimals < 0) decimals = 0;
    if (decimals > 9) decimals = 9;
    std::snprintf(buf, sizeof buf, "%.*f", decimals, value);
    return trim_zeros(buf);
}

std::string format_si(double value, const std::string& unit, int digits) {
    if (!std::isfinite(value)) return "--";
    if (value == 0) return "0 " + unit;
    static const char* prefixes[] = {"p", "n", "\xC2\xB5", "m", "", "k", "M", "G"};
    int idx = static_cast<int>(std::floor(std::log10(std::fabs(value)) / 3.0)) + 4;
    if (idx < 0) idx = 0;
    if (idx > 7) idx = 7;
    double scaled = value / std::pow(10.0, (idx - 4) * 3);
    // rounding can push 999.95 up to 1000: move to the next prefix
    const std::string probe = format_plain(scaled, digits);
    if (std::fabs(std::strtod(probe.c_str(), nullptr)) >= 1000.0 && idx < 7) {
        ++idx;
        scaled /= 1000.0;
    }
    return format_plain(scaled, digits) + " " + prefixes[idx] + unit;
}

}  // namespace ohm
