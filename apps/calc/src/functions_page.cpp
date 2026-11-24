#include "functions_page.h"

namespace calc_ui {
namespace {

namespace key = platform::key;

constexpr int kRowH = 33;
constexpr int kRowGap = 1;
constexpr int kFooterY = 138;
constexpr int kTextX = 34;

}  // namespace

void FunctionsPage::build(lv_obj_t* parent) {
    const auto& s = ctx_.style;
    for (size_t i = 0; i < rows_.size(); ++i) {
        auto& r = rows_[i];
        const int y = static_cast<int>(i) * (kRowH + kRowGap) + 2;
        r.box = make_box(parent, 4, y, kWidth - 8, kRowH);
        lv_obj_set_style_radius(r.box, 4, 0);
        lv_obj_set_style_bg_color(r.box, s.surface, 0);
        lv_obj_set_style_border_color(r.box, s.accent, 0);
        lv_obj_set_style_clip_corner(r.box, true, 0);

        auto* swatch = make_box(r.box, 0, 0, 4, kRowH);
        lv_obj_set_style_bg_color(swatch, s.plot[i], 0);
        lv_obj_set_style_bg_opa(swatch, LV_OPA_COVER, 0);

        char name[8];
        lv_snprintf(name, sizeof(name), "y%u=", static_cast<unsigned>(i + 1));
        r.name = make_label(r.box, s.ui12, s.plot[i], name);
        lv_obj_set_pos(r.name, 8, 4);

        r.text = make_label(r.box, s.mono14, s.text);
        lv_label_set_long_mode(r.text, LV_LABEL_LONG_MODE_CLIP);
        lv_obj_set_pos(r.text, kTextX, 2);

        r.cursor = make_box(r.box, kTextX, 3, 2, 17);
        lv_obj_set_style_bg_color(r.cursor, s.accent, 0);

        r.error = make_label(r.box, s.ui10, s.error);
        lv_label_set_recolor(r.error, true);
        lv_obj_set_width(r.error, kWidth - 8 - kTextX - 4);
        lv_label_set_long_mode(r.error, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_pos(r.error, kTextX, 19);
    }

    auto* footer = make_label(parent, s.ui10, s.dim,
                              "OK next/plot  SPC complete  Fn+F/X row  TAB graph");
    lv_obj_set_pos(footer, 8, kFooterY + 2);

    load_from_engine();
}

void FunctionsPage::load_from_engine() {
    for (size_t i = 0; i < edit_.size(); ++i) edit_[i].set(ctx_.engine.plots[i]);
    refresh_all();
}

void FunctionsPage::on_show() { refresh_all(); }

void FunctionsPage::on_engine_changed() { load_from_engine(); }

void FunctionsPage::commit_row(size_t i) {
    if (ctx_.engine.plots[i] != edit_[i].text()) {
        ctx_.engine.plots[i] = edit_[i].text();
        ctx_.mark_dirty();
    }
}

void FunctionsPage::refresh_row(size_t i) {
    const auto& s = ctx_.style;
    auto& r = rows_[i];
    const bool active = i == current_;
    lv_obj_set_style_bg_opa(r.box, active ? LV_OPA_COVER : LV_OPA_40, 0);
    lv_obj_set_style_border_width(r.box, active ? 1 : 0, 0);
    lv_obj_set_style_bg_opa(r.cursor, active ? LV_OPA_COVER : LV_OPA_TRANSP, 0);

    const auto& text = edit_[i].text();
    lv_label_set_text(r.text, text.empty() && !active ? "-" : text.c_str());
    lv_obj_set_style_text_color(r.text, text.empty() ? s.dim : s.text, 0);
    lv_obj_set_style_text_opa(r.name, ctx_.engine.plot_enabled[i] ? LV_OPA_COVER : LV_OPA_40, 0);

    // keep the cursor visible inside the row
    const int cw = lv_font_get_glyph_width(s.mono14, '0', '0');
    const int visible = kWidth - 8 - kTextX - 8;
    const int cursor_x = static_cast<int>(edit_[i].cursor()) * cw;
    const int offset = cursor_x > visible ? cursor_x - visible : 0;
    lv_obj_set_x(r.text, kTextX - offset);
    lv_obj_set_x(r.cursor, kTextX - offset + cursor_x - 1);

    // the active row shows name suggestions while an identifier is typed
    if (active) {
        const auto sug = completer_.suggestions(edit_[i], ctx_.engine.env);
        if (!sug.empty()) {
            const size_t current = completer_.cycling() ? completer_.index() : 0;
            char accent[8];
            lv_snprintf(accent, sizeof(accent), "%06x", static_cast<unsigned>(lv_color_to_u32(s.accent) & 0xffffff));
            std::string line = "SPC ";
            for (size_t k = 0; k < sug.size(); ++k) {
                const std::string name = sug[k].name + (sug[k].call ? "(" : "");
                line += k == current ? std::string("#") + accent + " " + name + "# " : name + " ";
            }
            lv_obj_set_style_text_color(r.error, s.dim, 0);
            lv_label_set_text(r.error, line.c_str());
            return;
        }
    }

    std::string error;
    if (!text.empty() && !ctx_.engine.compile_plot(text, &error)) {
        lv_label_set_text(r.error, error.c_str());
    } else if (!text.empty() && !ctx_.engine.plot_enabled[i]) {
        lv_obj_set_style_text_color(r.error, s.dim, 0);
        lv_label_set_text(r.error, "hidden on graph");
        return;
    } else {
        lv_label_set_text(r.error, "");
    }
    lv_obj_set_style_text_color(r.error, s.error, 0);
}

void FunctionsPage::refresh_all() {
    for (size_t i = 0; i < rows_.size(); ++i) refresh_row(i);
}

bool FunctionsPage::on_key(const cz::KeyEvent& ev) {
    if (!ev.pressed_or_repeat()) return false;
    auto& ed = edit_[current_];
    const size_t prev = current_;
    switch (ev.key) {
        case key::Up:
            if (current_ > 0) --current_;
            break;
        case key::Down:
            if (current_ + 1 < rows_.size()) ++current_;
            break;
        case key::Enter:
            if (ev.phase != cz::KeyPhase::Press) return true;
            commit_row(current_);
            if (current_ + 1 < rows_.size() && !edit_[current_].empty()) {
                ++current_;
            } else {
                ctx_.go(PageId::Graph);
                return true;
            }
            break;
        case key::Esc:
            if (ev.phase != cz::KeyPhase::Press) return true;
            commit_row(current_);
            ctx_.go(PageId::Calc);
            return true;
        case key::Backspace: ed.smart_backspace(); break;
        case key::Delete: ed.del(); break;
        case key::Left: ed.left(); break;
        case key::Right: ed.right(); break;
        case key::Home: ed.home(); break;
        case key::End: ed.end(); break;
        default:
            if (!key::is_printable(ev.key)) return false;
            if (ev.key == ' ') {
                if (!completer_.accept(ed, ctx_.engine.env)) ed.insert(' ');
                commit_row(current_);
                refresh_row(current_);
                return true;
            }
            ed.type(static_cast<char>(ev.key));
            break;
    }
    completer_.reset();
    commit_row(prev);
    refresh_row(prev);
    refresh_row(current_);
    return true;
}

}  // namespace calc_ui
