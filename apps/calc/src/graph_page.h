#pragma once

#include "app_context.h"
#include "plot_math.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace calc_ui {

// Function plotter with pan/zoom, trace, root and extremum search.
class GraphPage : public Page {
public:
    static constexpr int kW = kWidth;
    static constexpr int kH = kHeight - kStatusHeight;

    explicit GraphPage(AppContext& ctx);

    void build(lv_obj_t* parent) override;
    void on_show() override;
    bool on_key(const cz::KeyEvent& ev) override;
    void on_engine_changed() override;

    // persisted by the app
    calc::Viewport viewport() const { return view_; }
    void set_viewport(const calc::Viewport& v) { view_ = v; }

private:
    struct Slot {
        std::optional<calc::Plot> plot;
        std::string error;
        std::vector<double> samples;  // y per pixel column
    };

    void compile();
    void render();
    void draw_grid();
    void draw_curve(size_t slot);
    void draw_trace();
    void update_labels();
    void update_readout();

    void put(int x, int y, uint32_t c);
    void hline(int y, uint32_t c);
    void vline(int x, uint32_t c);
    void line(int x0, int y0, int x1, int y1, uint32_t c);

    double px_to_x(double px) const { return view_.xmin + px * view_.width() / kW; }
    double x_to_px(double x) const { return (x - view_.xmin) / view_.width() * kW; }
    double y_to_py(double y) const { return (view_.ymax - y) / view_.height() * kH; }

    int first_visible_slot(int from, int dir) const;
    void trace_to(double x);
    void find_root();
    void find_extremum();

    AppContext& ctx_;
    calc::Viewport view_;
    std::array<Slot, calc::Engine::kPlotSlots> slots_{};
    std::vector<uint32_t> pixels_;

    bool trace_{false};
    int trace_slot_{0};
    double trace_x_{0};
    bool show_legend_{true};

    lv_obj_t* canvas_{nullptr};
    lv_obj_t* legend_{nullptr};
    lv_obj_t* readout_{nullptr};
    lv_obj_t* empty_{nullptr};
    std::vector<lv_obj_t*> tick_labels_;
};

}  // namespace calc_ui
