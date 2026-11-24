#include "plot_math.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace calc {

Viewport Viewport::square(double xmin, double xmax, int px_w, int px_h) {
    Viewport v;
    v.xmin = xmin;
    v.xmax = xmax;
    const double half = (xmax - xmin) * px_h / px_w / 2.0;
    v.ymin = -half;
    v.ymax = half;
    return v;
}

void Viewport::pan(double dx_fraction, double dy_fraction) {
    const double dx = width() * dx_fraction;
    const double dy = height() * dy_fraction;
    xmin += dx;
    xmax += dx;
    ymin += dy;
    ymax += dy;
}

void Viewport::zoom(double factor, double cx, double cy) {
    const double w = width() / factor;
    const double h = height() / factor;
    // keep (cx, cy) at the same relative screen position
    const double rx = (cx - xmin) / width();
    const double ry = (cy - ymin) / height();
    xmin = cx - rx * w;
    xmax = xmin + w;
    ymin = cy - ry * h;
    ymax = ymin + h;
}

void Viewport::center_y(double y) {
    const double h = height();
    ymin = y - h / 2;
    ymax = y + h / 2;
}

double nice_step(double range, int target) {
    if (!(range > 0) || target <= 0) return 1;
    const double raw = range / target;
    const double mag = std::pow(10.0, std::floor(std::log10(raw)));
    const double r = raw / mag;
    const double nice = r < 1.5 ? 1 : r < 3.5 ? 2 : r < 7.5 ? 5 : 10;
    return nice * mag;
}

namespace {

double bisect(const Fn& f, double a, double b, double fa) {
    for (int i = 0; i < 100; ++i) {
        const double m = (a + b) / 2;
        if (m == a || m == b) break;
        const double fm = f(m);
        if (fm == 0) return m;
        if ((fm < 0) == (fa < 0)) {
            a = m;
            fa = fm;
        } else {
            b = m;
        }
    }
    return (a + b) / 2;
}

double golden(const Fn& f, double a, double b, bool maximize) {
    const double g = (std::sqrt(5.0) - 1) / 2;
    auto val = [&](double x) { return maximize ? -f(x) : f(x); };
    double c = b - g * (b - a);
    double d = a + g * (b - a);
    double fc = val(c);
    double fd = val(d);
    for (int i = 0; i < 120 && std::fabs(b - a) > 1e-13 * std::max(1.0, std::fabs(a)); ++i) {
        if (fc < fd) {
            b = d;
            d = c;
            fd = fc;
            c = b - g * (b - a);
            fc = val(c);
        } else {
            a = c;
            c = d;
            fc = fd;
            d = a + g * (b - a);
            fd = val(d);
        }
    }
    return (a + b) / 2;
}

// scans outward from the sample closest to x0: 0, +1, -1, +2, -2 ...
template <typename Visit>
std::optional<double> scan_outward(int n, int start, Visit visit) {
    for (int dist = 0; dist < n; ++dist) {
        for (int sign : {1, -1}) {
            if (dist == 0 && sign == -1) continue;
            const int i = start + sign * dist;
            if (i < 1 || i >= n - 1) continue;
            if (auto r = visit(i)) return r;
        }
    }
    return std::nullopt;
}

}  // namespace

std::optional<double> root_near(const Fn& f, double x0, double lo, double hi, int samples) {
    if (samples < 3 || !(hi > lo)) return std::nullopt;
    std::vector<double> xs(static_cast<size_t>(samples)), ys(static_cast<size_t>(samples));
    for (int i = 0; i < samples; ++i) {
        xs[static_cast<size_t>(i)] = lo + (hi - lo) * i / (samples - 1);
        ys[static_cast<size_t>(i)] = f(xs[static_cast<size_t>(i)]);
    }
    const int start = static_cast<int>(std::lround((x0 - lo) / (hi - lo) * (samples - 1)));
    // check the interval [i, i+1] and [i-1, i] around each visited sample
    return scan_outward(samples, start, [&](int i) -> std::optional<double> {
        for (int j : {i, i - 1}) {
            const double a = xs[static_cast<size_t>(j)], b = xs[static_cast<size_t>(j + 1)];
            const double fa = ys[static_cast<size_t>(j)], fb = ys[static_cast<size_t>(j + 1)];
            if (!std::isfinite(fa) || !std::isfinite(fb)) continue;
            if (fa == 0) return a;
            if ((fa < 0) != (fb < 0)) {
                const double r = bisect(f, a, b, fa);
                // reject poles: a real root has a small |f| at the end
                const double fr = f(r);
                if (std::isfinite(fr) && std::fabs(fr) < 1e-6 * std::max({1.0, std::fabs(fa), std::fabs(fb)})) {
                    return r;
                }
            }
        }
        return std::nullopt;
    });
}

std::optional<double> extremum_near(const Fn& f, double x0, double lo, double hi, int samples, bool* is_max) {
    if (samples < 3 || !(hi > lo)) return std::nullopt;
    std::vector<double> xs(static_cast<size_t>(samples)), ys(static_cast<size_t>(samples));
    for (int i = 0; i < samples; ++i) {
        xs[static_cast<size_t>(i)] = lo + (hi - lo) * i / (samples - 1);
        ys[static_cast<size_t>(i)] = f(xs[static_cast<size_t>(i)]);
    }
    const int start = static_cast<int>(std::lround((x0 - lo) / (hi - lo) * (samples - 1)));
    return scan_outward(samples, start, [&](int i) -> std::optional<double> {
        const double a = ys[static_cast<size_t>(i - 1)], b = ys[static_cast<size_t>(i)], c = ys[static_cast<size_t>(i + 1)];
        if (!std::isfinite(a) || !std::isfinite(b) || !std::isfinite(c)) return std::nullopt;
        const bool max = b >= a && b > c;
        const bool min = b <= a && b < c;
        if (!max && !min) return std::nullopt;
        if (is_max) *is_max = max;
        const double x = golden(f, xs[static_cast<size_t>(i - 1)], xs[static_cast<size_t>(i + 1)], max);
        // golden section stalls at ~sqrt(eps) on a flat top; refine on f' = 0
        const double h = 1e-3 * std::max(1.0, std::fabs(x));
        auto df = [&](double t) { return (f(t + h) - f(t - h)) / (2 * h); };
        const double span = 1e-6 * std::max(1.0, std::fabs(x));
        const double lo_x = x - span, hi_x = x + span;
        const double dlo = df(lo_x), dhi = df(hi_x);
        if (std::isfinite(dlo) && std::isfinite(dhi) && (dlo < 0) != (dhi < 0)) {
            return bisect(df, lo_x, hi_x, dlo);
        }
        return x;
    });
}

}  // namespace calc
