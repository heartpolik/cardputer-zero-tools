#include "crc_page.h"

#include <algorithm>
#include <cstdio>

namespace bit_ui {
namespace {

namespace key = platform::key;

constexpr int kInputY = 2;
constexpr int kInputH = 24;
constexpr int kMessageY = 27;
constexpr int kRowsY = 40;
constexpr int kRowH = 14;
constexpr int kPadX = 8;

}  // namespace

void CrcPage::build(lv_obj_t* parent) {
    const auto& s = ctx_.style;
    input_.set(ctx_.session.crc_input);

    input_box_ = make_box(parent, 6, kInputY, kWidth - 12, kInputH);
    lv_obj_set_style_bg_color(input_box_, s.surface, 0);
    lv_obj_set_style_bg_opa(input_box_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(input_box_, 1, 0);
    lv_obj_set_style_border_color(input_box_, s.accent, 0);
    lv_obj_set_style_radius(input_box_, 4, 0);
    lv_obj_set_style_clip_corner(input_box_, true, 0);
    input_label_ = make_label(input_box_, s.mono14, s.text);
    lv_label_set_long_mode(input_label_, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_width(input_label_, LV_SIZE_CONTENT);
    cursor_ = make_box(input_box_, 0, 4, 2, kInputH - 10);
    lv_obj_set_style_bg_color(cursor_, s.accent, 0);
    lv_obj_set_style_bg_opa(cursor_, LV_OPA_COVER, 0);

    message_ = make_label(parent, s.ui10, s.dim);
    lv_obj_set_pos(message_, kPadX, kMessageY);
    lv_obj_set_width(message_, kWidth - 2 * kPadX);
    lv_label_set_long_mode(message_, LV_LABEL_LONG_MODE_DOTS);

    for (int i = 0; i < kVisibleRows; ++i) {
        rows_[i] = make_box(parent, 4, kRowsY + i * kRowH, kWidth - 8, kRowH);
        lv_obj_set_style_bg_color(rows_[i], s.selection, 0);
        lv_obj_set_style_radius(rows_[i], 2, 0);
        row_text_[i] = make_label(rows_[i], s.mono12, s.text);
        lv_label_set_recolor(row_text_[i], true);
        lv_obj_set_pos(row_text_[i], 4, -1);
    }
    refresh();
}

void CrcPage::on_show() { refresh(); }

void CrcPage::refresh() {
    parsed_ = bits::parse_bytes(input_.text());
    refresh_input();
    refresh_rows();
}

void CrcPage::refresh_input() {
    if (!input_label_) return;
    const auto& s = ctx_.style;
    if (input_.empty()) {
        const std::string hint = "#" + color_hex(s.dim) + " 01 03 00 00 00 0A   or  \"text\\r\\n\"#";
        lv_label_set_recolor(input_label_, true);
        lv_label_set_text(input_label_, hint.c_str());
    } else {
        lv_label_set_recolor(input_label_, false);
        lv_label_set_text(input_label_, input_.text().c_str());
    }
    const int cw = glyph_width(s.mono14);
    const int visible = kWidth - 12 - 14;
    const int cursor_x = static_cast<int>(input_.cursor()) * cw;
    const int offset = cursor_x > visible ? cursor_x - visible : 0;
    const int font_h = lv_font_get_line_height(s.mono14);
    lv_obj_set_pos(input_label_, 6 - offset, (kInputH - 2 - font_h) / 2);
    lv_obj_set_pos(cursor_, 6 - offset + cursor_x - 1, 4);
}

void CrcPage::refresh_rows() {
    if (!rows_[0]) return;
    const auto& s = ctx_.style;
    const auto& specs = bits::checksums();
    const int n = static_cast<int>(specs.size());
    int& sel = ctx_.session.crc_selected;
    sel = std::clamp(sel, 0, n - 1);
    if (sel < first_) first_ = sel;
    if (sel >= first_ + kVisibleRows) first_ = sel - kVisibleRows + 1;
    first_ = std::clamp(first_, 0, std::max(0, n - kVisibleRows));

    const std::string dim = "#" + color_hex(s.dim) + " ";
    const std::string acc = "#" + color_hex(s.accent) + " ";

    if (!parsed_.ok) {
        lv_obj_set_style_text_color(message_, s.error, 0);
        lv_label_set_text_fmt(message_, "! %s", parsed_.error.c_str());
    } else {
        lv_obj_set_style_text_color(message_, s.dim, 0);
        const auto& spec = specs[static_cast<size_t>(sel)];
        lv_label_set_text_fmt(message_, "%d byte%s%s  |  %s",
                              static_cast<int>(parsed_.bytes.size()), parsed_.bytes.size() == 1 ? "" : "s",
                              parsed_.text ? " (text)" : "", spec.note);
    }

    for (int i = 0; i < kVisibleRows; ++i) {
        const int idx = first_ + i;
        if (idx >= n) {
            lv_obj_add_flag(rows_[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(rows_[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_opa(rows_[i], idx == sel ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        const auto& spec = specs[static_cast<size_t>(idx)];
        char line[160];
        if (parsed_.ok && !parsed_.bytes.empty()) {
            const uint32_t v = bits::compute(spec, parsed_.bytes);
            const int digits = spec.width == 0 ? 2 : spec.width / 4;
            char value[16];
            std::snprintf(value, sizeof value, "0x%0*X", digits, v);
            // name | value | bytes in wire order
            std::snprintf(line, sizeof line, "%-14s%s%-11s# %swire %s#", spec.name, acc.c_str(), value, dim.c_str(),
                          bits::wire_bytes(spec, v).c_str());
        } else {
            std::snprintf(line, sizeof line, "%-14s%s-#", spec.name, dim.c_str());
        }
        lv_label_set_text(row_text_[i], line);
    }
}

bool CrcPage::on_key(const cz::KeyEvent& ev) {
    if (!ev.pressed_or_repeat()) return false;
    const int n = static_cast<int>(bits::checksums().size());
    int& sel = ctx_.session.crc_selected;
    switch (ev.key) {
        case key::Up:
            sel = (sel + n - 1) % n;
            ctx_.mark_dirty();
            refresh_rows();
            return true;
        case key::Down:
            sel = (sel + 1) % n;
            ctx_.mark_dirty();
            refresh_rows();
            return true;
        case key::Enter: {
            if (ev.phase != cz::KeyPhase::Press) return true;
            if (!parsed_.ok || parsed_.bytes.empty()) return true;
            const auto& spec = bits::checksums()[static_cast<size_t>(sel)];
            const uint32_t v = bits::compute(spec, parsed_.bytes);
            const auto& env = ctx_.session.env;
            if (spec.width > env.word.width) {
                ctx_.rt.toast().show("Word is " + std::to_string(env.word.width) + "-bit: F3 to widen",
                                     view::widgets::ToastTone::Default, 1500);
            }
            ctx_.send_to_calc(bits::literal(v, bits::Word{64, false}, env.input));
            return true;
        }
        case key::Backspace:
            input_.backspace();
            break;
        case key::Delete:
            input_.del();
            break;
        case key::Left:
            input_.left();
            break;
        case key::Right:
            input_.right();
            break;
        case key::Home:
            input_.home();
            break;
        case key::End:
            input_.end();
            break;
        case key::Esc:
            if (ev.phase != cz::KeyPhase::Press) return true;
            if (input_.empty()) return false;
            input_.clear();
            break;
        default:
            if (!platform::key::is_printable(ev.key)) return false;
            input_.insert(static_cast<char>(ev.key));
            break;
    }
    ctx_.session.crc_input = input_.text();
    ctx_.mark_dirty();
    refresh();
    return true;
}

}  // namespace bit_ui
