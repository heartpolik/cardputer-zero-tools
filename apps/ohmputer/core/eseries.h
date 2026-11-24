// IEC 60063 preferred values (E6..E96) and helpers (pure C++).
#pragma once

#include <string>
#include <vector>

namespace ohm {

enum class Series { E6, E12, E24, E48, E96 };

const char* series_name(Series s);
Series next_series(Series s);
double series_tolerance(Series s);  // 0.2, 0.1, 0.05, 0.02, 0.01

// mantissas in [100, 1000): E24 = 100 110 120 ... 910
const std::vector<int>& series_values(Series s);
const std::vector<int>& e96();  // index 0 = EIA-96 code 01

// nearest preferred value (log scale); dir -1 = at most v, +1 = at least v, 0 = closest
double nearest(double v, Series s, int dir = 0);
// is v (within 0.1%) a value of the series?
bool in_series(double v, Series s);

struct DividerPair {
    double r1{0}, r2{0}, ratio{0};  // ratio = r2 / (r1 + r2)
};
// R1 (top) / R2 (bottom) from the series closest to the ratio, R2 in 1k..100k
DividerPair divider_pair(double ratio, Series s);

}  // namespace ohm
