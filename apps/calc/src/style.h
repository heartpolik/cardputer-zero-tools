#pragma once

#include "asset_manager.h"
#include "lvgl.h"

#include <array>

namespace calc_ui {

constexpr int kWidth = 320;
constexpr int kHeight = 170;
constexpr int kStatusHeight = 16;

struct Style {
    bool dark{true};
    lv_color_t bg, surface, border, text, dim, accent, error, warn, selection;
    lv_color_t grid, grid_major, axis;
    std::array<lv_color_t, 4> plot;

    const lv_font_t* mono12{nullptr};
    const lv_font_t* mono14{nullptr};
    const lv_font_t* mono18{nullptr};
    const lv_font_t* mono_bold18{nullptr};
    const lv_font_t* ui10{nullptr};
    const lv_font_t* ui12{nullptr};

    static Style make(bool dark, app::AssetManager& assets);
};

// small helpers for plain (style-less) LVGL objects
lv_obj_t* make_box(lv_obj_t* parent, int x, int y, int w, int h);
lv_obj_t* make_label(lv_obj_t* parent, const lv_font_t* font, lv_color_t color, const char* text = "");

}  // namespace calc_ui
