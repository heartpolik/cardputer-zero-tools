#include "calc_page.h"

#include <algorithm>

namespace bit_ui {
namespace {

namespace key = platform::key;

constexpr int kInputY = 2;
constexpr int kInputH = 24;
constexpr int kMessageY = 27;
constexpr int kValuesY = 40;
constexpr int kRowStep = 15;
constexpr int kGridY = 101;
constexpr int kGridRowStep = 26;
constexpr int kLabelX = 8;
constexpr int kValueX = 44;

const char* kNames[3] = {"HEX", "DEC", "OCT"};

// calculator conveniences: a leading binary operator continues from ans, open parens close
std::string prepare(const std::string& line) {
    std::string s = cz::prepare_input(line);  // + * / % ^ ** -> ans..., auto-close
    size_t i = s.find_first_not_of(' ');
    if (i == std::string::npos) return s;
    if (s[i] == '&' || s[i] == '|' || s.compare(i, 2, "<<") == 0 || s.compare(i, 2, ">>") == 0) s.insert(i, "ans");
    return s;
}

}  // namespace

void CalcPage::build(lv_obj_t* parent) {
    const auto& s = ctx_.style;

    input_box_ = make_box(parent, 6, kInputY, kWidth - 12, kInputH);
    lv_obj_set_style_bg_color(input_box_, s.surface, 0);
    lv_obj_set_style_bg_opa(input_box_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(input_box_, 1, 0);
    lv_obj_set_style_radius(input_box_, 4, 0);
    lv_obj_set_style_clip_corner(input_box_, true, 0);
    input_label_ = make_label(input_box_, s.mono18, s.text);
    lv_label_set_long_mode(input_label_, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_width(input_label_, LV_SIZE_CONTENT);
    cursor_ = make_box(input_box_, 0, 4, 2, kInputH - 10);
    lv_obj_set_style_bg_color(cursor_, s.accent, 0);

    message_label_ = make_label(parent, s.ui10, s.dim);
    lv_obj_set_pos(message_label_, kLabelX, kMessageY);
    lv_obj_set_width(message_label_, kWidth - 2 * kLabelX);
    lv_label_set_long_mode(message_label_, LV_LABEL_LONG_MODE_DOTS);

    for (int i = 0; i < 4; ++i) {
        const int y = kValuesY + i * kRowStep;
        names_[i] = make_label(parent, s.ui10, s.dim, i < 3 ? kNames[i] : "");
        lv_obj_set_pos(names_[i], kLabelX, y + 3);
        values_[i] = make_label(parent, s.mono14, s.text);
        lv_obj_set_pos(values_[i], kValueX, y);
        extras_[i] = make_label(parent, s.mono12, s.dim);
        lv_obj_align(extras_[i], LV_ALIGN_TOP_RIGHT, -kLabelX, y + 2);
    }
    lv_obj_set_width(values_[3], kWidth - kValueX - kLabelX);
    lv_label_set_long_mode(values_[3], LV_LABEL_LONG_MODE_DOTS);

    grid_box_ = make_box(parent, 0, kGridY, kWidth, kContentH - kGridY);
    grid_cursor_ = make_box(grid_box_, 0, 0, 8, 15);
    lv_obj_set_style_bg_color(grid_cursor_, s.selection, 0);
    lv_obj_set_style_border_color(grid_cursor_, s.accent, 0);
    lv_obj_set_style_border_width(grid_cursor_, 1, 0);
    lv_obj_set_style_radius(grid_cursor_, 2, 0);
    for (int r = 0; r < 2; ++r) {
        grid_rows_[r] = make_label(grid_box_, s.mono12, s.dim);
        lv_label_set_recolor(grid_rows_[r], true);
        for (int n = 0; n < 8; ++n) grid_index_[r][n] = make_label(grid_box_, s.ui10, s.border);
    }

    refresh();
}

void CalcPage::on_show() { refresh(); }

void CalcPage::on_mode_changed() {
    bit_ = std::min(bit_, ctx_.session.env.word.width - 1);
    refresh();
}

void CalcPage::set_input(const std::string& text) {
    grid_ = false;
    hist_pos_ = -1;
    input_.set(text);
    message_.clear();
    refresh();
}

uint64_t CalcPage::shown_value() const { return live_ok_ ? live_value_ : ctx_.session.ans(); }

void CalcPage::refresh() {
    live_ok_ = false;
    live_error_.clear();
    live_assign_.clear();
    live_error_pos_ = std::string::npos;
    if (!input_.empty()) {
        const auto r = bits::evaluate(prepare(input_.text()), ctx_.session.env);
        live_ok_ = r.ok;
        live_value_ = r.value;
        live_error_ = r.error;
        live_error_pos_ = r.error_pos;
        live_assign_ = r.assigned;
    }
    refresh_input();
    refresh_values();
    refresh_grid();
    refresh_message();
}

void CalcPage::refresh_input() {
    if (!input_label_) return;
    const auto& s = ctx_.style;
    lv_label_set_text(input_label_, input_.text().c_str());
    const int cw = glyph_width(s.mono18);
    const int visible = kWidth - 12 - 14;
    const int cursor_x = static_cast<int>(input_.cursor()) * cw;
    const int offset = cursor_x > visible ? cursor_x - visible : 0;
    const int font_h = lv_font_get_line_height(s.mono18);
    lv_obj_set_pos(input_label_, 6 - offset, (kInputH - 2 - font_h) / 2);
    lv_obj_set_pos(cursor_, 6 - offset + cursor_x - 1, 4);
    lv_obj_set_style_bg_opa(cursor_, grid_ ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(input_box_, grid_ ? s.border : s.accent, 0);
}

void CalcPage::refresh_values() {
    if (!values_[0]) return;
    const auto& s = ctx_.style;
    const auto& w = ctx_.session.env.word;
    const uint64_t v = w.wrap(shown_value());
    // accent: live result of the input, text: ans, dim: input has an error (stale ans)
    const lv_color_t color = live_ok_ ? s.accent : input_.empty() ? s.text : s.dim;

    lv_label_set_text(values_[0], bits::format_hex(v, w.width).c_str());
    lv_label_set_text(values_[1], bits::format_dec(v, w, w.is_signed).c_str());
    lv_label_set_text(values_[2], bits::format_oct(v).c_str());
    for (int i = 0; i < 3; ++i) lv_obj_set_style_text_color(values_[i], color, 0);

    // extras: popcount, the other signedness, bit length
    lv_label_set_text_fmt(extras_[0], "%d set", __builtin_popcountll(v));
    const std::string other = bits::format_dec(v, w, !w.is_signed);
    const std::string primary = bits::format_dec(v, w, w.is_signed);
    const int room = (kWidth - kValueX - kLabelX) / glyph_width(s.mono12) -
                     static_cast<int>(primary.size()) * glyph_width(s.mono14) / glyph_width(s.mono12) - 2;
    if (other != primary && static_cast<int>(other.size()) + 2 <= room) {
        lv_label_set_text_fmt(extras_[1], "%s %s", w.is_signed ? "u" : "s", other.c_str());
    } else {
        lv_label_set_text(extras_[1], "");
    }
    lv_label_set_text(extras_[2], v ? ("msb " + std::to_string(63 - __builtin_clzll(v))).c_str() : "");

    lv_label_set_text(names_[3], bits::decode_name(ctx_.session.decode));
    lv_label_set_text(values_[3], bits::decode(ctx_.session.decode, v, w).c_str());
    lv_obj_set_style_text_color(values_[3], s.warn, 0);
    lv_label_set_text(extras_[3], "");
}

void CalcPage::refresh_grid() {
    if (!grid_box_) return;
    const auto& s = ctx_.style;
    const auto& w = ctx_.session.env.word;
    const uint64_t v = w.wrap(shown_value());
    const int cw = glyph_width(s.mono12);
    const int per_row = std::min(w.width, 32);
    const int rows = w.width / per_row;
    const int chars = per_row + per_row / 4 - 1;
    const int x0 = (kWidth - chars * cw) / 2;
    const std::string one = "#" + color_hex(s.accent) + " ";

    for (int r = 0; r < 2; ++r) {
        const bool used = r < rows;
        if (!used) {
            lv_obj_add_flag(grid_rows_[r], LV_OBJ_FLAG_HIDDEN);
            for (auto* l : grid_index_[r]) lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(grid_rows_[r], LV_OBJ_FLAG_HIDDEN);
        const int top = w.width - 1 - r * per_row;  // bit index of the first char
        std::string text;
        for (int c = 0; c < per_row; ++c) {
            const int b = top - c;
            if (c > 0 && c % 4 == 0) text += ' ';
            if ((v >> b) & 1) text += one + "1#";
            else text += '0';
        }
        lv_label_set_text(grid_rows_[r], text.c_str());
        const int y = r * kGridRowStep;
        lv_obj_set_pos(grid_rows_[r], x0, y);
        for (int n = 0; n < 8; ++n) {
            auto* l = grid_index_[r][n];
            if (n >= per_row / 4) {
                lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
                continue;
            }
            lv_obj_clear_flag(l, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(l, "%d", top - n * 4);
            lv_obj_set_pos(l, x0 + n * 5 * cw, y + 14);
        }
    }

    if (grid_) {
        const int r = (w.width - 1 - bit_) / per_row;
        const int c = (w.width - 1 - bit_) % per_row;
        lv_obj_set_pos(grid_cursor_, x0 + (c + c / 4) * cw - 1, r * kGridRowStep);
        lv_obj_set_size(grid_cursor_, cw + 2, 16);
        lv_obj_set_style_bg_opa(grid_cursor_, LV_OPA_COVER, 0);
        lv_obj_set_style_border_opa(grid_cursor_, LV_OPA_COVER, 0);
    } else {
        lv_obj_set_style_bg_opa(grid_cursor_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_opa(grid_cursor_, LV_OPA_TRANSP, 0);
    }
}

void CalcPage::refresh_message() {
    if (!message_label_) return;
    const auto& s = ctx_.style;
    lv_obj_set_style_text_color(message_label_, s.dim, 0);
    if (grid_) {
        const uint64_t v = ctx_.session.env.word.wrap(shown_value());
        lv_label_set_text_fmt(message_label_, "BIT %d = %d   SPC flip  0/1 set  I invert  ESC back", bit_,
                              static_cast<int>((v >> bit_) & 1));
        return;
    }
    if (!input_.empty() && !live_ok_) {
        lv_obj_set_style_text_color(message_label_, s.error, 0);
        lv_label_set_text_fmt(message_label_, "! %s", live_error_.c_str());
        return;
    }
    if (!live_assign_.empty()) {
        lv_label_set_text_fmt(message_label_, "Enter: %s = ...", live_assign_.c_str());
        return;
    }
    if (hist_pos_ >= 0) {
        lv_label_set_text_fmt(message_label_, "history %d/%d", hist_pos_ + 1,
                              static_cast<int>(ctx_.session.history.size()));
        return;
    }
    if (!message_.empty()) {
        lv_label_set_text(message_label_, message_.c_str());
        return;
    }
    lv_label_set_text(message_label_, input_.empty() ? "ans   Fn+X bits   Fn+F history   F1 help" : "");
}

void CalcPage::submit() {
    if (input_.empty()) return;
    const std::string prepared = prepare(input_.text());
    const auto r = ctx_.session.submit(prepared);
    if (!r.ok) {
        input_.set(prepared);
        if (r.error_pos != std::string::npos) input_.move_to(r.error_pos);
        refresh();
        return;
    }
    message_ = r.assigned.empty() ? "= ans" : r.assigned + " = ans";
    input_.clear();
    hist_pos_ = -1;
    ctx_.mark_dirty();
    refresh();
}

void CalcPage::history_step(int dir) {
    const auto& hist = ctx_.session.history;
    if (hist.empty()) return;
    if (hist_pos_ < 0) {
        if (dir > 0) return;
        draft_ = input_.text();
        hist_pos_ = static_cast<int>(hist.size()) - 1;
    } else {
        hist_pos_ += dir;
        if (hist_pos_ < 0) hist_pos_ = 0;
        if (hist_pos_ >= static_cast<int>(hist.size())) {
            hist_pos_ = -1;
            input_.set(draft_);
            refresh();
            return;
        }
    }
    input_.set(hist[static_cast<size_t>(hist_pos_)].input);
    refresh();
}

void CalcPage::edit_value(uint64_t value) {
    const auto& env = ctx_.session.env;
    input_.set(bits::literal(value, env.word, env.input));
    hist_pos_ = -1;
    refresh();
}

bool CalcPage::on_grid_key(const cz::KeyEvent& ev) {
    const auto& w = ctx_.session.env.word;
    const uint64_t v = w.wrap(shown_value());
    const bool press = ev.phase == cz::KeyPhase::Press;
    switch (ev.key) {
        case key::Left:
        case 'z':
        case ',':
            bit_ = std::min(bit_ + 1, w.width - 1);
            break;
        case key::Right:
        case 'c':
        case '.':
            bit_ = std::max(bit_ - 1, 0);
            break;
        case key::Up:
        case 'f':
            if (w.width == 64 && bit_ < 32) bit_ += 32;
            else grid_ = false;
            break;
        case key::Down:
        case 'x':
            if (w.width == 64 && bit_ >= 32) bit_ -= 32;
            break;
        case key::Home:
            bit_ = w.width - 1;
            break;
        case key::End:
            bit_ = 0;
            break;
        case key::Esc:
        case key::Tab:
            if (press) grid_ = false;
            if (ev.key == key::Tab) return false;
            break;
        case ' ':
        case key::Enter:
            if (press) edit_value(v ^ (uint64_t{1} << bit_));
            break;
        case '0':
            if (press) edit_value(v & ~(uint64_t{1} << bit_));
            break;
        case '1':
            if (press) edit_value(v | (uint64_t{1} << bit_));
            break;
        case 'i':
        case '~':
            if (press) edit_value(~v);
            break;
        default:
            return true;
    }
    refresh();
    return true;
}

bool CalcPage::on_key(const cz::KeyEvent& ev) {
    if (!ev.pressed_or_repeat()) return false;
    if (grid_) return on_grid_key(ev);
    const uint32_t k = ev.key;
    switch (k) {
        case key::Up:
            history_step(-1);
            return true;
        case key::Down:
            if (hist_pos_ >= 0) {
                history_step(1);
            } else {
                grid_ = true;
                refresh();
            }
            return true;
        case key::Enter:
            if (ev.phase == cz::KeyPhase::Press) submit();
            return true;
        case key::Backspace:
            input_.smart_backspace();
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
            if (input_.empty() && message_.empty()) return false;  // hold Esc quits
            input_.clear();
            break;
        default:
            if (!platform::key::is_printable(k)) return false;
            input_.type(static_cast<char>(k));
            break;
    }
    hist_pos_ = -1;
    message_.clear();
    refresh();
    return true;
}

}  // namespace bit_ui
