#include "format.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace calc {
namespace {

void trim_fraction(std::string& s) {
    if (s.find('.') == std::string::npos) return;
    while (!s.empty() && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
}

// Splits |value| rounded to `digits` significant digits into mantissa digits and exponent.
int decimal_exponent(double value, int digits) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*e", digits - 1, value);
    const char* e = std::strchr(buf, 'e');
    return e ? std::atoi(e + 1) : 0;
}

std::string fixed(double value, int decimals) {
    char buf[512];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals < 0 ? 0 : decimals, value);
    std::string s(buf);
    trim_fraction(s);
    if (s == "-0") s = "0";
    return s;
}

std::string mantissa_exp(double value, int exp, int digits, int mantissa_int_digits) {
    const double mantissa = value / std::pow(10.0, exp);
    std::string m = fixed(mantissa, digits - mantissa_int_digits);
    if (exp == 0) return m;
    return m + "e" + std::to_string(exp);
}

}  // namespace

const char* format_name(NumberFormat f) {
    switch (f) {
        case NumberFormat::Auto: return "AUTO";
        case NumberFormat::Sci: return "SCI";
        case NumberFormat::Eng: return "ENG";
    }
    return "?";
}

std::string format_number(double value, NumberFormat format, int digits) {
    if (std::isnan(value)) return "undefined";
    if (std::isinf(value)) return value > 0 ? "inf" : "-inf";
    if (value == 0) return "0";

    const int exp = decimal_exponent(value, digits);

    switch (format) {
        case NumberFormat::Auto:
            if (exp >= -9 && exp < 12) return fixed(value, digits - 1 - exp);
            return mantissa_exp(value, exp, digits, 1);
        case NumberFormat::Sci: {
            std::string s = mantissa_exp(value, exp, digits, 1);
            if (exp == 0) s += "e0";
            return s;
        }
        case NumberFormat::Eng: {
            int exp3 = static_cast<int>(std::floor(exp / 3.0)) * 3;
            const int int_digits = exp - exp3 + 1;  // 1..3 digits before the point
            std::string s = mantissa_exp(value, exp3, digits, int_digits);
            // rounding can push the mantissa to 1000 (e.g. 999.9999999999999)
            const double m = std::fabs(std::strtod(s.substr(0, s.find('e')).c_str(), nullptr));
            if (m >= 1000.0) {
                exp3 += 3;
                s = mantissa_exp(value, exp3, digits, 1);
            }
            return s;
        }
    }
    return fixed(value, digits);
}

std::string format_short(double value) {
    if (value == 0 || std::fabs(value) < 1e-12) return "0";
    const int exp = decimal_exponent(value, 4);
    if (exp >= -3 && exp < 5) return fixed(value, 3 - exp);
    return mantissa_exp(value, exp, 3, 1);
}

}  // namespace calc
