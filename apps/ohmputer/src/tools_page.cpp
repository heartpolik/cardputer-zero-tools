#include "tools_page.h"

#include "si.h"

#include <algorithm>
#include <cctype>

namespace ohm_ui {
namespace {

namespace key = platform::key;

constexpr int kHeaderH = 18;

}  // namespace

ohm::Form& ToolsPage::form() { return ctx_.session.forms[static_cast<size_t>(ctx_.session.tool)]; }

const char* ToolsPage::status() const { return ohm::tools()[static_cast<size_t>(ctx_.session.tool)].name; }

void ToolsPage::build(lv_obj_t* parent) {
    area_ = make_box(parent, 0, 0, kWidth, kContentH);
    refresh();
}

void ToolsPage::select(int dir) {
    const auto& fields = form().tool().fields;
    const int n = static_cast<int>(fields.size());
    for (int step = 0; step < n; ++step) {
        selected_ = (selected_ + dir + n) % n;
        if (!fields[static_cast<size_t>(selected_)].output) return;
    }
}

void ToolsPage::refresh() {
    if (!area_) return;
    const auto& s = ctx_.style;
    lv_obj_clean(area_);
    auto& f = form();
    const auto& tool = f.tool();
    const int n = static_cast<int>(tool.fields.size());
    if (selected_ >= n || tool.fields[static_cast<size_t>(selected_)].output) {
        selected_ = 0;
    }
    const auto sol = f.solve(ctx_.session.series);

    // header: < title >  i/n
    auto* title = make_label(area_, s.ui12, s.accent);
    lv_label_set_text_fmt(title, "<  %s  >", tool.title);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);
    auto* index = make_label(area_, s.ui10, s.dim);
    lv_label_set_text_fmt(index, "%d/%d", ctx_.session.tool + 1, static_cast<int>(ohm::tools().size()));
    lv_obj_align(index, LV_ALIGN_TOP_RIGHT, -6, 3);

    const bool compact = n > 6;
    const int row_h = compact ? 14 : 18;
    const lv_font_t* font = compact ? s.mono12 : s.mono14;
    int y = kHeaderH;
    for (int i = 0; i < n; ++i, y += row_h) {
        const auto& fd = tool.fields[static_cast<size_t>(i)];
        const bool sel = i == selected_;
        auto* row = make_box(area_, 4, y, kWidth - 8, row_h);
        if (sel) {
            lv_obj_set_style_bg_color(row, s.selection, 0);
            lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(row, 2, 0);
        }
        auto* label = make_label(row, compact ? s.ui10 : s.ui12, fd.output ? s.dim : s.text, fd.label);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 4, 0);

        std::string text;
        lv_color_t color = s.dim;
        if (f.is_set(static_cast<size_t>(i))) {
            const std::string& t = f.text(static_cast<size_t>(i));
            // "12 V" but "20mA", "4k7Ω" when the text ends in a prefix
            text = t + (std::isalpha(static_cast<unsigned char>(t.back())) ? "" : " ") + fd.unit;
            color = s.text;
        } else if (sol.values[static_cast<size_t>(i)]) {
            const double v = *sol.values[static_cast<size_t>(i)];
            if (std::string(fd.unit) == "h") text = ohm::format_hours(v);
            else if (std::string(fd.unit) == "%" || std::string(fd.unit) == "Wh") text = ohm::format_plain(v, 4) + " " + fd.unit;
            else text = ohm::format_si(v, fd.unit, 4);
            color = s.accent;
        } else {
            text = "-";
        }
        auto* value = make_label(row, font, color, text.c_str());
        lv_obj_align(value, LV_ALIGN_RIGHT_MID, -10, 0);
        if (sel) {
            // typing cursor after the text
            auto* cur = make_box(row, kWidth - 8 - 7, 3, 2, row_h - 6);
            lv_obj_set_style_bg_color(cur, s.accent, 0);
            lv_obj_set_style_bg_opa(cur, LV_OPA_COVER, 0);
        }
    }

    // notes / error
    y += 2;
    if (!sol.error.empty()) {
        auto* e = make_label(area_, s.mono12, s.error, ("! " + sol.error).c_str());
        lv_obj_set_pos(e, 8, y);
        y += 13;
    }
    for (const auto& note : sol.notes) {
        if (y > kContentH - 12) break;
        auto* l = make_label(area_, s.mono12, s.warn, note.c_str());
        lv_obj_set_pos(l, 8, y);
        y += 13;
    }
}

bool ToolsPage::on_key(const cz::KeyEvent& ev) {
    if (!ev.pressed_or_repeat()) return false;
    auto& f = form();
    const size_t i = static_cast<size_t>(selected_);
    const int tools = static_cast<int>(ohm::tools().size());
    switch (ev.key) {
        case key::Up:
            select(-1);
            break;
        case key::Down:
        case key::Enter:
            select(1);
            break;
        case key::Left:
            ctx_.session.tool = (ctx_.session.tool + tools - 1) % tools;
            selected_ = 0;
            ctx_.status_changed();
            break;
        case key::Right:
            ctx_.session.tool = (ctx_.session.tool + 1) % tools;
            selected_ = 0;
            ctx_.status_changed();
            break;
        case key::Backspace: {
            std::string t = f.text(i);
            if (!t.empty()) t.pop_back();  // typed text is ASCII
            f.set_text(i, t);
            break;
        }
        case key::Delete:
            f.set_text(i, "");
            break;
        case key::Esc:
            if (ev.phase != cz::KeyPhase::Press) return true;
            if (!f.is_set(i)) return false;
            f.set_text(i, "");
            break;
        default:
            if (!platform::key::is_printable(ev.key) || f.text(i).size() >= 14) return false;
            f.set_text(i, f.text(i) + static_cast<char>(ev.key));
            break;
    }
    ctx_.mark_dirty();
    refresh();
    return true;
}

}  // namespace ohm_ui
