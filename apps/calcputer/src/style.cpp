#include "style.h"

namespace calc_ui {

Style Style::make(bool dark, app::AssetManager& assets) {
    Style s;
    s.dark = dark;
    if (dark) {
        s.bg = lv_color_hex(0x0e0f12);
        s.surface = lv_color_hex(0x1a1c21);
        s.border = lv_color_hex(0x2c2f36);
        s.text = lv_color_hex(0xe8e8ea);
        s.dim = lv_color_hex(0x80858f);
        s.accent = lv_color_hex(0x63e2b7);
        s.error = lv_color_hex(0xf08080);
        s.warn = lv_color_hex(0xf2c97d);
        s.selection = lv_color_hex(0x243a33);
        s.grid = lv_color_hex(0x1c1e23);
        s.grid_major = lv_color_hex(0x2a2d34);
        s.axis = lv_color_hex(0x6b707a);
        s.plot = {lv_color_hex(0x63e2b7), lv_color_hex(0x70c0e8), lv_color_hex(0xf2c97d), lv_color_hex(0xf08ab0)};
    } else {
        s.bg = lv_color_hex(0xfafafa);
        s.surface = lv_color_hex(0xffffff);
        s.border = lv_color_hex(0xd8d9de);
        s.text = lv_color_hex(0x1f2225);
        s.dim = lv_color_hex(0x7a7f88);
        s.accent = lv_color_hex(0x168a50);
        s.error = lv_color_hex(0xc8303f);
        s.warn = lv_color_hex(0xb07300);
        s.selection = lv_color_hex(0xdcf3e6);
        s.grid = lv_color_hex(0xeeeff2);
        s.grid_major = lv_color_hex(0xdcdee3);
        s.axis = lv_color_hex(0x8a8f98);
        s.plot = {lv_color_hex(0x168a50), lv_color_hex(0x1f6fd0), lv_color_hex(0xc77700), lv_color_hex(0xc8306a)};
    }

    auto mono = [&](const char* file, uint32_t size, const lv_font_t* fallback) -> const lv_font_t* {
        const lv_font_t* f = assets.load_font(file, size);
        return f ? f : fallback;
    };
    s.mono12 = mono("JetBrainsMono-Regular.ttf", 12, &lv_font_montserrat_12);
    s.mono14 = mono("JetBrainsMono-Regular.ttf", 14, &lv_font_montserrat_14);
    s.mono18 = mono("JetBrainsMono-Regular.ttf", 18, &lv_font_montserrat_18);
    s.mono_bold18 = mono("JetBrainsMono-Bold.ttf", 18, &lv_font_montserrat_18);
    s.ui10 = mono("JetBrainsMono-Bold.ttf", 10, &lv_font_montserrat_10);
    s.ui12 = mono("JetBrainsMono-Bold.ttf", 12, &lv_font_montserrat_12);
    return s;
}

lv_obj_t* make_box(lv_obj_t* parent, int x, int y, int w, int h) {
    auto* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}

lv_obj_t* make_label(lv_obj_t* parent, const lv_font_t* font, lv_color_t color, const char* text) {
    auto* l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_label_set_text(l, text);
    return l;
}

}  // namespace calc_ui
