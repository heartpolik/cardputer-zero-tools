#pragma once

#include "runtime.h"
#include "session.h"
#include "ui_style.h"

#include <functional>
#include <string>

namespace ohm_ui {

using cz::kHeight;
using cz::kStatusHeight;
using cz::kWidth;
using cz::make_box;
using cz::make_label;
using cz::Style;

constexpr int kContentH = kHeight - kStatusHeight;  // 154

enum class PageId { Mark = 0, Bands = 1, Tools = 2 };
constexpr int kPages = 3;

struct AppContext {
    cz::Runtime& rt;
    ohm::Session& session;
    const Style& style;
    std::function<void()> mark_dirty;
    std::function<void()> status_changed;
};

class Page {
public:
    virtual ~Page() = default;
    virtual void build(lv_obj_t* parent) = 0;
    virtual void on_show() {}
    virtual bool on_key(const cz::KeyEvent& ev) = 0;
    virtual void on_series_changed() {}
    virtual const char* status() const { return ""; }  // right side of the status bar
};

inline int glyph_width(const lv_font_t* font) {
    const int w = lv_font_get_glyph_width(font, '0', '0');
    return w > 0 ? w : 8;
}

// colour of a component kind letter
lv_color_t kind_color(const Style& s, char kind);

}  // namespace ohm_ui
