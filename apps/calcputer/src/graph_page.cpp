#include "graph_page.h"

#include "format.h"

#include <algorithm>
#include <cmath>

namespace calc_ui {
namespace {

namespace key = platform::key;

constexpr int kMaxTickLabels = 24;
constexpr double kZoomStep = 1.5;
constexpr double kPanStep = 0.125;

uint32_t rgb(lv_color_t c) { return 0xff000000u | (lv_color_to_u32(c) & 0x00ffffffu); }

}  // namespace

GraphPage::GraphPage(AppContext& ctx)
    : ctx_(ctx), view_(calc::Viewport::square(-10, 10, kW, kH)), pixels_(static_cast<size_t>(kW * kH)) {}

void GraphPage::build(lv_obj_t* parent) {
    const auto& s = ctx_.style;
    canvas_ = lv_canvas_create(parent);
    lv_canvas_set_buffer(canvas_, pixels_.data(), kW, kH, LV_COLOR_FORMAT_XRGB8888);
    lv_obj_set_pos(canvas_, 0, 0);

    for (int i = 0; i < kMaxTickLabels; ++i) {
        auto* l = make_label(parent, s.ui10, s.dim);
        lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
        tick_labels_.push_back(l);
    }

    legend_ = make_label(parent, s.mono12, s.text);
    lv_label_set_recolor(legend_, true);
    lv_obj_set_pos(legend_, 4, 3);
    lv_obj_set_style_bg_color(legend_, s.bg, 0);
    lv_obj_set_style_bg_opa(legend_, LV_OPA_80, 0);
    lv_obj_set_style_pad_hor(legend_, 4, 0);
    lv_obj_set_style_pad_ver(legend_, 2, 0);
    lv_obj_set_style_radius(legend_, 3, 0);

    readout_ = make_label(parent, s.mono12, s.text);
    lv_label_set_recolor(readout_, true);
    lv_obj_set_width(readout_, kW);
    lv_obj_set_pos(readout_, 0, kH - 16);
    lv_obj_set_style_bg_color(readout_, s.surface, 0);
    lv_obj_set_style_bg_opa(readout_, LV_OPA_90, 0);
    lv_obj_set_style_pad_hor(readout_, 6, 0);
    lv_obj_set_style_pad_ver(readout_, 1, 0);
    lv_label_set_long_mode(readout_, LV_LABEL_LONG_MODE_CLIP);

    empty_ = make_label(parent, s.mono12, s.dim, "No functions yet.\nTAB to Y= and type e.g. sin(x)");
    lv_obj_set_style_text_align(empty_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(empty_);
}

void GraphPage::on_show() {
    compile();
    render();
}

void GraphPage::on_engine_changed() {
    compile();
    render();
}

void GraphPage::compile() {
    const auto& engine = ctx_.engine;
    for (size_t i = 0; i < slots_.size(); ++i) {
        auto& slot = slots_[i];
        slot.error.clear();
        slot.plot.reset();
        if (engine.plots[i].empty()) continue;
        slot.plot = engine.compile_plot(engine.plots[i], &slot.error);
    }
    if (trace_ && (!slots_[static_cast<size_t>(trace_slot_)].plot ||
                   !engine.plot_enabled[static_cast<size_t>(trace_slot_)])) {
        const int next = first_visible_slot(0, 1);
        if (next < 0) trace_ = false;
        else trace_slot_ = next;
    }
}

int GraphPage::first_visible_slot(int from, int dir) const {
    const int n = static_cast<int>(slots_.size());
    for (int k = 0; k < n; ++k) {
        const int i = ((from + dir * k) % n + n) % n;
        if (slots_[static_cast<size_t>(i)].plot && ctx_.engine.plot_enabled[static_cast<size_t>(i)]) return i;
    }
    return -1;
}

// ------------------------------------------------------------------ drawing

void GraphPage::put(int x, int y, uint32_t c) {
    if (x < 0 || y < 0 || x >= kW || y >= kH) return;
    pixels_[static_cast<size_t>(y * kW + x)] = c;
}

void GraphPage::hline(int y, uint32_t c) {
    if (y < 0 || y >= kH) return;
    std::fill_n(pixels_.begin() + y * kW, kW, c);
}

void GraphPage::vline(int x, uint32_t c) {
    if (x < 0 || x >= kW) return;
    for (int y = 0; y < kH; ++y) pixels_[static_cast<size_t>(y * kW + x)] = c;
}

void GraphPage::line(int x0, int y0, int x1, int y1, uint32_t c) {
    // Bresenham with a 2px pen (vertical thickening reads well for curves)
    const int dx = std::abs(x1 - x0);
    const int dy = -std::abs(y1 - y0);
    const int sx = x0 < x1 ? 1 : -1;
    const int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (int guard = 0; guard < 4 * (kW + kH); ++guard) {
        put(x0, y0, c);
        put(x0, y0 + 1, c);
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void GraphPage::draw_grid() {
    const auto& s = ctx_.style;
    std::fill(pixels_.begin(), pixels_.end(), rgb(s.bg));

    const double xstep = calc::nice_step(view_.width(), 8);
    const double ystep = calc::nice_step(view_.height(), 5);
    const uint32_t grid = rgb(s.grid_major);

    for (double x = std::ceil(view_.xmin / xstep) * xstep; x <= view_.xmax; x += xstep) {
        vline(static_cast<int>(std::lround(x_to_px(x))), grid);
    }
    for (double y = std::ceil(view_.ymin / ystep) * ystep; y <= view_.ymax; y += ystep) {
        hline(static_cast<int>(std::lround(y_to_py(y))), grid);
    }
    const uint32_t axis = rgb(s.axis);
    if (view_.xmin <= 0 && view_.xmax >= 0) vline(static_cast<int>(std::lround(x_to_px(0))), axis);
    if (view_.ymin <= 0 && view_.ymax >= 0) hline(static_cast<int>(std::lround(y_to_py(0))), axis);
}

void GraphPage::draw_curve(size_t i) {
    auto& slot = slots_[i];
    slot.samples.assign(kW, std::nan(""));
    if (!slot.plot || !ctx_.engine.plot_enabled[i]) return;

    const uint32_t color = rgb(ctx_.style.plot[i]);
    for (int px = 0; px < kW; ++px) {
        slot.samples[static_cast<size_t>(px)] = ctx_.engine.eval_plot(*slot.plot, px_to_x(px + 0.5));
    }
    bool have_prev = false;
    double prev_py = 0;
    for (int px = 0; px < kW; ++px) {
        const double y = slot.samples[static_cast<size_t>(px)];
        if (!std::isfinite(y)) {
            have_prev = false;
            continue;
        }
        const double py = y_to_py(y);
        if (have_prev) {
            const bool both_off_opposite = (prev_py < 0 && py > kH) || (prev_py > kH && py < 0);
            const bool jump = std::fabs(py - prev_py) > 2.0 * kH;
            if (!both_off_opposite && !jump) {
                const auto clamp = [](double v) { return static_cast<int>(std::lround(std::clamp(v, -2.0 * kH, 3.0 * kH))); };
                line(px - 1, clamp(prev_py), px, clamp(py), color);
            }
        } else {
            put(px, static_cast<int>(std::lround(py)), color);
        }
        prev_py = py;
        have_prev = true;
    }
}

void GraphPage::draw_trace() {
    if (!trace_) return;
    const auto& slot = slots_[static_cast<size_t>(trace_slot_)];
    if (!slot.plot) return;
    const double y = ctx_.engine.eval_plot(*slot.plot, trace_x_);
    const int px = static_cast<int>(std::lround(x_to_px(trace_x_)));
    const uint32_t dim = rgb(ctx_.style.dim);
    for (int yy = 0; yy < kH; yy += 3) put(px, yy, dim);  // dotted vertical
    if (!std::isfinite(y)) return;
    const int py = static_cast<int>(std::lround(y_to_py(y)));
    for (int xx = 0; xx < kW; xx += 3) put(xx, py, dim);
    const uint32_t c = rgb(ctx_.style.text);
    for (int d = -3; d <= 3; ++d) {
        put(px + d, py - 3, c);
        put(px + d, py + 3, c);
        put(px - 3, py + d, c);
        put(px + 3, py + d, c);
    }
}

void GraphPage::render() {
    if (!canvas_) return;
    draw_grid();
    bool any = false;
    for (size_t i = 0; i < slots_.size(); ++i) {
        draw_curve(i);
        any = any || (slots_[i].plot && ctx_.engine.plot_enabled[i]);
    }
    draw_trace();
    lv_obj_invalidate(canvas_);

    bool any_text = false;
    for (const auto& p : ctx_.engine.plots) any_text = any_text || !p.empty();
    if (any_text) lv_obj_add_flag(empty_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(empty_, LV_OBJ_FLAG_HIDDEN);
    (void)any;

    update_labels();
    update_readout();
}

void GraphPage::update_labels() {
    const auto& s = ctx_.style;
    size_t used = 0;
    auto place = [&](const std::string& text, int x, int y, lv_align_t) {
        if (used >= tick_labels_.size()) return;
        auto* l = tick_labels_[used++];
        lv_label_set_text(l, text.c_str());
        lv_obj_update_layout(l);
        const int w = lv_obj_get_width(l);
        const int h = lv_obj_get_height(l);
        lv_obj_set_pos(l, std::clamp(x, 1, kW - w - 1), std::clamp(y, 1, kH - h - 1));
        lv_obj_clear_flag(l, LV_OBJ_FLAG_HIDDEN);
    };

    // x labels along the x axis (or the bottom edge when the axis is off-screen)
    const double xstep = calc::nice_step(view_.width(), 8);
    const double ystep = calc::nice_step(view_.height(), 5);
    const int axis_py = std::clamp(static_cast<int>(std::lround(y_to_py(0))), 0, kH - 12);
    const int axis_px = std::clamp(static_cast<int>(std::lround(x_to_px(0))), 0, kW - 30);
    int n = 0;
    for (double x = std::ceil(view_.xmin / xstep) * xstep; x <= view_.xmax; x += xstep) {
        if (std::fabs(x) < xstep / 2) continue;
        if (++n % 2 == 0 && view_.width() / xstep > 6) continue;  // every other label
        const int px = static_cast<int>(std::lround(x_to_px(x)));
        place(calc::format_short(x), px - 8, axis_py + 2, LV_ALIGN_TOP_MID);
    }
    for (double y = std::ceil(view_.ymin / ystep) * ystep; y <= view_.ymax; y += ystep) {
        if (std::fabs(y) < ystep / 2) continue;
        const int py = static_cast<int>(std::lround(y_to_py(y)));
        place(calc::format_short(y), axis_px + 3, py - 6, LV_ALIGN_LEFT_MID);
    }
    for (size_t i = used; i < tick_labels_.size(); ++i) lv_obj_add_flag(tick_labels_[i], LV_OBJ_FLAG_HIDDEN);

    // legend with recolored slot names
    std::string legend;
    for (size_t i = 0; i < slots_.size(); ++i) {
        const auto& src = ctx_.engine.plots[i];
        if (src.empty()) continue;
        char hex[8];
        lv_snprintf(hex, sizeof(hex), "%06x", static_cast<unsigned>(lv_color_to_u32(s.plot[i]) & 0xffffff));
        std::string body = src.size() > 22 ? src.substr(0, 21) + "~" : src;
        if (!slots_[i].plot) body = "error";
        else if (!ctx_.engine.plot_enabled[i]) body = "(hidden)";
        if (!legend.empty()) legend += "\n";
        legend += std::string("#") + hex + " y" + std::to_string(i + 1) + "# " + body;
    }
    lv_label_set_text(legend_, legend.c_str());
    if (legend.empty() || !show_legend_) lv_obj_add_flag(legend_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(legend_, LV_OBJ_FLAG_HIDDEN);
}

void GraphPage::update_readout() {
    if (!trace_) {
        char buf[96];
        lv_snprintf(buf, sizeof(buf), "x %s..%s  y %s..%s  T trace",
                    calc::format_short(view_.xmin).c_str(), calc::format_short(view_.xmax).c_str(),
                    calc::format_short(view_.ymin).c_str(), calc::format_short(view_.ymax).c_str());
        lv_label_set_text(readout_, buf);
        return;
    }
    const auto& slot = slots_[static_cast<size_t>(trace_slot_)];
    const double y = slot.plot ? ctx_.engine.eval_plot(*slot.plot, trace_x_) : std::nan("");
    double d = std::nan("");
    if (slot.plot) {
        d = calc::numeric_derivative([&](double x) { return ctx_.engine.eval_plot(*slot.plot, x); }, trace_x_);
    }
    char hex[8];
    lv_snprintf(hex, sizeof(hex), "%06x",
                static_cast<unsigned>(lv_color_to_u32(ctx_.style.plot[static_cast<size_t>(trace_slot_)]) & 0xffffff));
    const std::string text = std::string("#") + hex + " y" + std::to_string(trace_slot_ + 1) + "# x=" +
                             calc::format_number(trace_x_, calc::NumberFormat::Auto, 6) +
                             " y=" + calc::format_number(y, calc::NumberFormat::Auto, 6) +
                             " y'=" + calc::format_number(d, calc::NumberFormat::Auto, 4);
    lv_label_set_text(readout_, text.c_str());
}

// ------------------------------------------------------------------ actions

void GraphPage::trace_to(double x) {
    trace_x_ = x;
    // follow the cursor horizontally
    if (trace_x_ < view_.xmin || trace_x_ > view_.xmax) {
        const double w = view_.width();
        view_.xmin = trace_x_ - w / 2;
        view_.xmax = trace_x_ + w / 2;
    }
    // and vertically when the point leaves the screen
    const auto& slot = slots_[static_cast<size_t>(trace_slot_)];
    if (slot.plot) {
        const double y = ctx_.engine.eval_plot(*slot.plot, trace_x_);
        if (std::isfinite(y) && (y < view_.ymin || y > view_.ymax)) view_.center_y(y);
    }
}

void GraphPage::find_root() {
    const auto& slot = slots_[static_cast<size_t>(trace_slot_)];
    if (!slot.plot) return;
    auto f = [&](double x) { return ctx_.engine.eval_plot(*slot.plot, x); };
    const auto r = calc::root_near(f, trace_x_, view_.xmin, view_.xmax, kW * 2);
    if (!r) {
        ctx_.rt.toast().show("No root on screen", view::widgets::ToastTone::Warning, 1500);
        return;
    }
    trace_to(*r);
    ctx_.rt.toast().show("Root x = " + calc::format_number(*r), view::widgets::ToastTone::Success, 2500);
}

void GraphPage::find_extremum() {
    const auto& slot = slots_[static_cast<size_t>(trace_slot_)];
    if (!slot.plot) return;
    auto f = [&](double x) { return ctx_.engine.eval_plot(*slot.plot, x); };
    bool is_max = false;
    const auto r = calc::extremum_near(f, trace_x_, view_.xmin, view_.xmax, kW * 2, &is_max);
    if (!r) {
        ctx_.rt.toast().show("No extremum on screen", view::widgets::ToastTone::Warning, 1500);
        return;
    }
    trace_to(*r);
    ctx_.rt.toast().show(std::string(is_max ? "Max" : "Min") + " at x = " + calc::format_number(*r, calc::NumberFormat::Auto, 8),
                         view::widgets::ToastTone::Success, 2500);
}

bool GraphPage::on_key(const cz::KeyEvent& ev) {
    if (!ev.pressed_or_repeat()) return false;
    const bool press = ev.phase == cz::KeyPhase::Press;
    uint32_t k = ev.key;

    // bare F/X/Z/C act as arrows here (no text entry on this page)
    switch (k) {
        case 'f': k = key::Up; break;
        case 'x': k = key::Down; break;
        case 'z': k = key::Left; break;
        case 'c': k = key::Right; break;
        default: break;
    }

    const double cx = trace_ ? trace_x_ : (view_.xmin + view_.xmax) / 2;
    const double cy = (view_.ymin + view_.ymax) / 2;

    switch (k) {
        case key::Left:
        case key::Right: {
            const double dir = k == key::Left ? -1.0 : 1.0;
            if (trace_) trace_to(trace_x_ + dir * view_.width() / kW);
            else view_.pan(dir * kPanStep, 0);
            break;
        }
        case key::Up:
        case key::Down:
            if (trace_) {
                if (!press) return true;
                const int next = first_visible_slot(trace_slot_ + (k == key::Up ? -1 : 1), k == key::Up ? -1 : 1);
                if (next >= 0) trace_slot_ = next;
                trace_to(trace_x_);
            } else {
                view_.pan(0, k == key::Up ? kPanStep : -kPanStep);
            }
            break;
        case 'i':
        case '+':
        case '=':
            view_.zoom(kZoomStep, cx, cy);
            break;
        case 'o':
        case '-':
            view_.zoom(1 / kZoomStep, cx, cy);
            break;
        case '0':
            if (!press) return true;
            view_ = calc::Viewport::square(-10, 10, kW, kH);
            break;
        case 'a': {
            if (!press) return true;
            // fit y to the visible curves
            double lo = INFINITY, hi = -INFINITY;
            for (size_t i = 0; i < slots_.size(); ++i) {
                if (!slots_[i].plot || !ctx_.engine.plot_enabled[i]) continue;
                for (double y : slots_[i].samples) {
                    if (std::isfinite(y) && std::fabs(y) < 1e12) {
                        lo = std::min(lo, y);
                        hi = std::max(hi, y);
                    }
                }
            }
            if (lo < hi) {
                const double pad = (hi - lo) * 0.08;
                view_.ymin = lo - pad;
                view_.ymax = hi + pad;
            } else if (lo == hi && std::isfinite(lo)) {
                view_.center_y(lo);
            }
            break;
        }
        case 's':
            if (!press) return true;
            {
                // square units around the current center
                const double mid = cy;
                view_.ymin = mid - view_.width() * kH / kW / 2;
                view_.ymax = mid + view_.width() * kH / kW / 2;
            }
            break;
        case 't':
        case key::Enter:
            if (!press) return true;
            if (trace_) {
                trace_ = false;
            } else {
                const int slot = first_visible_slot(trace_slot_, 1);
                if (slot < 0) {
                    ctx_.rt.toast().show("Nothing to trace", view::widgets::ToastTone::Warning, 1500);
                    return true;
                }
                trace_ = true;
                trace_slot_ = slot;
                trace_to((view_.xmin + view_.xmax) / 2);
            }
            break;
        case 'r':
            if (!press) return true;
            if (!trace_) on_key({'t', cz::KeyPhase::Press, ev.mods});
            find_root();
            break;
        case 'm':
            if (!press) return true;
            if (!trace_) on_key({'t', cz::KeyPhase::Press, ev.mods});
            find_extremum();
            break;
        case 'l':
            if (!press) return true;
            show_legend_ = !show_legend_;
            break;
        case '1':
        case '2':
        case '3':
        case '4': {
            if (!press) return true;
            const size_t i = k - '1';
            ctx_.engine.plot_enabled[i] = !ctx_.engine.plot_enabled[i];
            ctx_.mark_dirty();
            compile();
            break;
        }
        case key::Esc:
            if (!press) return true;
            if (trace_) trace_ = false;
            else ctx_.go(PageId::Calc);
            break;
        default:
            return false;
    }
    render();
    return true;
}

}  // namespace calc_ui
