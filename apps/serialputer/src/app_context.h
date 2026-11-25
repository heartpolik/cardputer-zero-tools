#pragma once

#include "engine.h"
#include "runtime.h"
#include "session.h"
#include "ui_style.h"

#include <functional>
#include <string>

namespace ser_ui {

using cz::kHeight;
using cz::kStatusHeight;
using cz::kWidth;
using cz::make_box;
using cz::make_label;
using cz::Style;

constexpr int kContentH = kHeight - kStatusHeight;  // 154
constexpr int kPageCount = 3;

enum class PageId { Term = 0, Modbus = 1, Port = 2 };

struct AppContext {
    cz::Runtime& rt;
    ser::Session& session;
    Engine& engine;
    const Style& style;
    std::function<void()> mark_dirty;
    std::function<void()> status_changed;
    std::function<void(const std::string&)> toast;
};

class Page {
public:
    virtual ~Page() = default;
    virtual void build(lv_obj_t* parent) = 0;
    virtual void on_show() {}
    virtual bool on_key(const cz::KeyEvent& ev) = 0;
    virtual void tick() {}  // ~20 times a second while visible
};

inline int glyph_width(const lv_font_t* font) {
    const int w = lv_font_get_glyph_width(font, '0', '0');
    return w > 0 ? w : 8;
}

inline std::string color_hex(lv_color_t c) {
    char buf[8];
    lv_snprintf(buf, sizeof(buf), "%06x", static_cast<unsigned>(lv_color_to_u32(c) & 0xffffff));
    return buf;
}

// a single-line edit box with a cursor (shared by the pages)
struct InputBox {
    lv_obj_t* box{nullptr};
    lv_obj_t* label{nullptr};
    lv_obj_t* cursor{nullptr};
    lv_obj_t* tag{nullptr};  // right-aligned mode tag
    int h{22};

    void build(lv_obj_t* parent, const Style& s, int x, int y, int w, int height) {
        h = height;
        box = make_box(parent, x, y, w, h);
        lv_obj_set_style_bg_color(box, s.surface, 0);
        lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(box, 1, 0);
        lv_obj_set_style_border_color(box, s.accent, 0);
        lv_obj_set_style_radius(box, 4, 0);
        lv_obj_set_style_clip_corner(box, true, 0);
        label = make_label(box, s.mono12, s.text);
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_CLIP);
        lv_obj_set_width(label, LV_SIZE_CONTENT);
        cursor = make_box(box, 0, 4, 2, h - 10);
        lv_obj_set_style_bg_color(cursor, s.accent, 0);
        lv_obj_set_style_bg_opa(cursor, LV_OPA_COVER, 0);
        tag = make_label(box, s.ui10, s.dim);
        lv_obj_align(tag, LV_ALIGN_RIGHT_MID, -4, 0);
    }

    // text (or a dim hint when empty) with the cursor kept in view
    void show(const Style& s, const std::string& text, size_t cursor_pos, const std::string& hint, int reserve_right) {
        if (text.empty()) {
            lv_obj_set_style_text_color(label, s.dim, 0);
            lv_label_set_text(label, hint.c_str());
        } else {
            lv_obj_set_style_text_color(label, s.text, 0);
            lv_label_set_text(label, text.c_str());
        }
        const int cw = glyph_width(s.mono12);
        const int visible = lv_obj_get_width(box) - 14 - reserve_right;
        const int cx = static_cast<int>(cursor_pos) * cw;
        const int offset = cx > visible ? cx - visible : 0;
        const int font_h = lv_font_get_line_height(s.mono12);
        lv_obj_set_pos(label, 6 - offset, (h - 2 - font_h) / 2);
        lv_obj_set_pos(cursor, 6 - offset + cx - 1, 4);
    }
};

}  // namespace ser_ui
