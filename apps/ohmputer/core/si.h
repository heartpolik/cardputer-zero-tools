// SI values: "4k7", "4.7k", "100n", "2u2", "0.5", "1e-3", "47R" (pure C++).
#pragma once

#include <optional>
#include <string>

namespace ohm {

// Parses a number with an optional SI prefix (p n u µ m k K M G), which may also
// stand in for the decimal point ("4k7" = 4700, "2u2" = 2.2e-6). 'R' works as a
// unit / decimal point ("4R7" = 4.7, "47R" = 47). Trailing unit letters
// (Ω ohm V A W F H Hz s %) are ignored. Case matters: m = milli, M = mega.
std::optional<double> parse_si(const std::string& text);

// 4700 -> "4.7 k" + unit; keeps `digits` significant digits, trims zeros.
std::string format_si(double value, const std::string& unit, int digits = 4);

// plain number with up to `digits` significant digits ("0.0473", "12.5")
std::string format_plain(double value, int digits = 4);

}  // namespace ohm
