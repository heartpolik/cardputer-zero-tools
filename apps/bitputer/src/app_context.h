#pragma once

#include "runtime.h"
#include "session.h"
#include "ui_style.h"

#include <functional>
#include <string>

namespace bit_ui {

using cz::kHeight;
using cz::kStatusHeight;
using cz::kWidth;
using cz::make_box;
using cz::make_label;
using cz::Style;

constexpr int kContentH = kHeight - kStatusHeight;  // 154

enum class PageId { Calc = 0, Crc = 1 };

struct AppContext {
    cz::Runtime& rt;
    bits::Session& session;
    const Style& style;
    std::function<void()> mark_dirty;
    std::function<void(PageId)> go;
    std::function<void(const std::string&)> send_to_calc;  // replaces the CALC input
};

class Page {
public:
    virtual ~Page() = default;
    virtual void build(lv_obj_t* parent) = 0;
    virtual void on_show() {}
    virtual bool on_key(const cz::KeyEvent& ev) = 0;
    virtual void on_mode_changed() {}  // word / base / decode changed
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

}  // namespace bit_ui
