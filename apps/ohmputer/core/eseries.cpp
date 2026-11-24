#include "eseries.h"

#include <cmath>

namespace ohm {
namespace {

const std::vector<int> kE96 = {
    100, 102, 105, 107, 110, 113, 115, 118, 121, 124, 127, 130, 133, 137, 140, 143, 147, 150, 154, 158,
    162, 165, 169, 174, 178, 182, 187, 191, 196, 200, 205, 210, 215, 221, 226, 232, 237, 243, 249, 255,
    261, 267, 274, 280, 287, 294, 301, 309, 316, 324, 332, 340, 348, 357, 365, 374, 383, 392, 402, 412,
    422, 432, 442, 453, 464, 475, 487, 499, 511, 523, 536, 549, 562, 576, 590, 604, 619, 634, 649, 665,
    681, 698, 715, 732, 750, 768, 787, 806, 825, 845, 866, 887, 909, 931, 953, 976};
const std::vector<int> kE24 = {100, 110, 120, 130, 150, 160, 180, 200, 220, 240, 270, 300,
                               330, 360, 390, 430, 470, 510, 560, 620, 680, 750, 820, 910};

std::vector<int> every_other(const std::vector<int>& v) {
    std::vector<int> out;
    for (size_t i = 0; i < v.size(); i += 2) out.push_back(v[i]);
    return out;
}

}  // namespace

const char* series_name(Series s) {
    switch (s) {
        case Series::E6: return "E6";
        case Series::E12: return "E12";
        case Series::E24: return "E24";
        case Series::E48: return "E48";
        case Series::E96: return "E96";
    }
    return "?";
}

Series next_series(Series s) {
    switch (s) {
        case Series::E6: return Series::E12;
        case Series::E12: return Series::E24;
        case Series::E24: return Series::E48;
        case Series::E48: return Series::E96;
        case Series::E96: return Series::E6;
    }
    return Series::E24;
}

double series_tolerance(Series s) {
    switch (s) {
        case Series::E6: return 0.2;
        case Series::E12: return 0.1;
        case Series::E24: return 0.05;
        case Series::E48: return 0.02;
        case Series::E96: return 0.01;
    }
    return 0.05;
}

const std::vector<int>& e96() { return kE96; }

const std::vector<int>& series_values(Series s) {
    static const std::vector<int> e48 = every_other(kE96);
    static const std::vector<int> e12 = every_other(kE24);
    static const std::vector<int> e6 = every_other(e12);
    switch (s) {
        case Series::E6: return e6;
        case Series::E12: return e12;
        case Series::E24: return kE24;
        case Series::E48: return e48;
        case Series::E96: return kE96;
    }
    return kE24;
}

double nearest(double v, Series s, int dir) {
    if (!(v > 0) || !std::isfinite(v)) return 0;
    const int decade = static_cast<int>(std::floor(std::log10(v))) - 2;
    double best = 0, best_err = 1e300;
    for (int d = decade - 1; d <= decade + 1; ++d) {
        const double scale = std::pow(10.0, d);
        for (int m : series_values(s)) {
            const double c = m * scale;
            if (dir < 0 && c > v * (1 + 1e-9)) continue;
            if (dir > 0 && c < v * (1 - 1e-9)) continue;
            const double err = std::fabs(std::log(c / v));
            if (err < best_err) {
                best_err = err;
                best = c;
            }
        }
    }
    return best;
}

bool in_series(double v, Series s) {
    const double n = nearest(v, s);
    return n > 0 && std::fabs(n / v - 1) < 1e-3;
}

DividerPair divider_pair(double ratio, Series s) {
    DividerPair best;
    if (!(ratio > 0 && ratio < 1)) return best;
    double best_err = 1e300;
    for (int d = 1; d <= 3; ++d) {  // R2 = 1k .. 100k (mantissas are x100)
        for (int m : series_values(s)) {
            const double r2 = m * std::pow(10.0, d);
            const double r1 = nearest(r2 * (1 / ratio - 1), s);
            const double got = r2 / (r1 + r2);
            // tiny bias towards R2 near 10k so equal-error pairs pick the friendliest value
            const double err = std::fabs(got / ratio - 1) + 1e-9 * std::fabs(std::log10(r2 / 1e4));
            if (err < best_err) {
                best_err = err;
                best = {r1, r2, got};
            }
        }
    }
    return best;
}

}  // namespace ohm
