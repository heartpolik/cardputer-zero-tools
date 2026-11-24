// Graph helpers: viewport, tick spacing, root / extremum search (pure, host-tested).
#pragma once

#include <functional>
#include <optional>

namespace calc {

using Fn = std::function<double(double)>;

struct Viewport {
    double xmin{-10}, xmax{10}, ymin{-5}, ymax{5};

    double width() const { return xmax - xmin; }
    double height() const { return ymax - ymin; }

    // y range from x range so one unit is equally long on both axes
    static Viewport square(double xmin, double xmax, int px_w, int px_h);

    void pan(double dx_fraction, double dy_fraction);
    void zoom(double factor, double cx, double cy);  // factor > 1 zooms in
    void center_y(double y);
};

// "Nice" tick step (1, 2 or 5 x 10^k) giving roughly `target` ticks over `range`.
double nice_step(double range, int target);

// Nearest sign change of f to x0 within [lo, hi], scanned with `samples` points,
// refined by bisection.
std::optional<double> root_near(const Fn& f, double x0, double lo, double hi, int samples);

// Nearest local extremum to x0 within [lo, hi]; *is_max reports the kind.
std::optional<double> extremum_near(const Fn& f, double x0, double lo, double hi, int samples,
                                    bool* is_max = nullptr);

}  // namespace calc
