#pragma once

#include <string>

namespace calc {

enum class NumberFormat { Auto, Sci, Eng };

const char* format_name(NumberFormat f);

// Formats a value with `digits` significant digits.
//  Auto: plain decimal for 1e-9 <= |v| < 1e12, scientific otherwise
//  Sci:  always d.ddd e N
//  Eng:  exponent is a multiple of 3
std::string format_number(double value, NumberFormat format = NumberFormat::Auto, int digits = 12);

// Short form for axis labels (up to 4 significant digits).
std::string format_short(double value);

}  // namespace calc
