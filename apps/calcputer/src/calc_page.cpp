#include "calc_page.h"

#include <algorithm>

namespace calc_ui {
namespace {

namespace key = platform::key;

constexpr int kContentH = kHeight - kStatusHeight;  // 154
constexpr int kHistoryH = 92;
constexpr int kPreviewY = 92;
constexpr int kInputY = 108;
constexpr int kInputH = 28;
constexpr int kFooterY = 138;
constexpr int kPadX = 8;
constexpr int kMaxRendered = 40;

int glyph_width(const lv_font_t* font) {
    const int w = lv_font_get_glyph_width(font, '0', '0');
    return w > 0 ? w : 8;
}

}  // namespace

void CalcPage::build(lv_obj_t* parent) {
    const auto& s = ctx_.style;

    history_ = lv_obj_create(parent);
    lv_obj_remove_style_all(history_);
    lv_obj_set_pos(history_, 0, 0);
    lv_obj_set_size(history_, kWidth, kHistoryH);
    lv_obj_set_flex_flow(history_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_ver(history_, 2, 0);
    lv_obj_set_scrollbar_mode(history_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(history_, LV_DIR_VER);
    lv_obj_clear_flag(history_, LV_OBJ_FLAG_CLICKABLE);

    empty_hint_ = make_label(parent, s.mono12, s.dim,
                             "2(3+4)^2   sin(30)   f(x)=x^2-2\n"
                             "solve(f(x),1)   integ(x^2,0,3)\n"
                             "Sym = blue legends: + T  - R  * 8  / N");
    lv_obj_set_style_text_align(empty_hint_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(empty_hint_, 4, 0);
    lv_obj_align(empty_hint_, LV_ALIGN_TOP_MID, 0, 22);

    preview_ = make_label(parent, s.mono14, s.dim);
    lv_obj_set_width(preview_, kWidth - 2 * kPadX);
    lv_obj_set_pos(preview_, kPadX, kPreviewY);
    lv_label_set_long_mode(preview_, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(preview_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_recolor(preview_, true);

    input_box_ = make_box(parent, 6, kInputY, kWidth - 12, kInputH);
    lv_obj_set_style_bg_color(input_box_, s.surface, 0);
    lv_obj_set_style_bg_opa(input_box_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(input_box_, s.accent, 0);
    lv_obj_set_style_border_width(input_box_, 1, 0);
    lv_obj_set_style_radius(input_box_, 4, 0);
    lv_obj_set_style_clip_corner(input_box_, true, 0);

    input_label_ = make_label(input_box_, s.mono18, s.text);
    lv_label_set_long_mode(input_label_, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_width(input_label_, LV_SIZE_CONTENT);

    cursor_ = make_box(input_box_, 0, 4, 2, kInputH - 10);
    lv_obj_set_style_bg_color(cursor_, s.accent, 0);
    lv_obj_set_style_bg_opa(cursor_, LV_OPA_COVER, 0);

    footer_ = make_label(parent, s.ui10, s.dim);
    lv_obj_set_width(footer_, kWidth - 2 * kPadX);
    lv_obj_set_pos(footer_, kPadX, kFooterY + 2);
    lv_label_set_long_mode(footer_, LV_LABEL_LONG_MODE_CLIP);

    rebuild_history();
    refresh_input();
    refresh_preview();
    refresh_footer();
}

void CalcPage::on_show() {
    refresh_preview();  // variables/functions may have changed elsewhere
}

void CalcPage::on_engine_changed() {
    rebuild_history();
    refresh_preview();
}

void CalcPage::insert(const std::string& text) {
    selected_ = -1;
    completer_.reset();
    input_.insert(text);
    message_.clear();
    refresh_selection();
    refresh_input();
    refresh_preview();
}

void CalcPage::set_message(const std::string& msg, bool error) {
    message_ = msg;
    message_error_ = error;
    refresh_preview();
}

void CalcPage::submit() {
    auto& engine = ctx_.engine;
    std::string line = input_.text();
    if (line.find_first_not_of(' ') == std::string::npos) {
        // empty Enter repeats the last calculation (e.g. "ans*2")
        if (engine.history.empty()) return;
        line = engine.history.back().input;
    }
    const std::string prepared = calc::prepare_input(line);
    const auto result = engine.submit(prepared);
    if (!result.ok) {
        // keep the input for fixing; jump to the error position when it maps back
        input_.set(prepared);
        if (result.error_pos != std::string::npos) input_.move_to(result.error_pos);
        set_message(result.text, true);
        refresh_input();
        return;
    }
    input_.clear();
    completer_.reset();
    message_.clear();
    selected_ = -1;
    ctx_.mark_dirty();
    rebuild_history();
    refresh_input();
    refresh_preview();
}

void CalcPage::rebuild_history() {
    if (!history_) return;
    const auto& s = ctx_.style;
    const auto& hist = ctx_.engine.history;
    lv_obj_clean(history_);

    if (hist.empty()) {
        lv_obj_clear_flag(empty_hint_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(empty_hint_, LV_OBJ_FLAG_HIDDEN);
    }

    const int total = static_cast<int>(hist.size());
    first_rendered_ = std::max(0, total - kMaxRendered);
    const int fit = (kWidth - 2 * kPadX) / glyph_width(s.mono14);

    for (int i = first_rendered_; i < total; ++i) {
        const auto& h = hist[static_cast<size_t>(i)];
        auto* row = lv_obj_create(history_);
        lv_obj_remove_style_all(row);
        lv_obj_set_width(row, kWidth);
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        lv_obj_set_style_pad_hor(row, kPadX, 0);
        lv_obj_set_style_pad_ver(row, 1, 0);
        lv_obj_set_style_bg_color(row, s.selection, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);

        const bool one_line = static_cast<int>(h.input.size() + h.output.size()) + 3 <= fit;
        auto* in = make_label(row, one_line ? s.mono14 : s.mono12, s.dim, h.input.c_str());
        auto* out = make_label(row, s.mono14, s.accent, h.output.c_str());
        lv_obj_set_style_text_align(out, LV_TEXT_ALIGN_RIGHT, 0);
        if (one_line) {
            lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        } else {
            lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_width(in, kWidth - 2 * kPadX);
            lv_obj_set_width(out, kWidth - 2 * kPadX);
            lv_label_set_long_mode(in, LV_LABEL_LONG_MODE_DOTS);
            lv_label_set_long_mode(out, LV_LABEL_LONG_MODE_DOTS);
        }
    }
    lv_obj_update_layout(history_);
    lv_obj_scroll_to_y(history_, LV_COORD_MAX, LV_ANIM_OFF);
    refresh_selection();
}

void CalcPage::refresh_selection() {
    if (!history_) return;
    const uint32_t n = lv_obj_get_child_count(history_);
    for (uint32_t i = 0; i < n; ++i) {
        auto* row = lv_obj_get_child(history_, static_cast<int32_t>(i));
        const bool sel = static_cast<int>(i) + first_rendered_ == selected_;
        lv_obj_set_style_bg_opa(row, sel ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        if (sel) lv_obj_scroll_to_view(row, LV_ANIM_OFF);
    }
    if (selected_ < 0 && n > 0) lv_obj_scroll_to_y(history_, LV_COORD_MAX, LV_ANIM_OFF);
    lv_obj_set_style_border_color(input_box_, selected_ < 0 ? ctx_.style.accent : ctx_.style.border, 0);
    lv_obj_set_style_bg_opa(cursor_, selected_ < 0 ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    refresh_footer();
}

void CalcPage::refresh_input() {
    if (!input_label_) return;
    lv_label_set_text(input_label_, input_.text().c_str());
    const int cw = glyph_width(ctx_.style.mono18);
    const int visible = kWidth - 12 - 14;  // box width minus padding
    const int cursor_x = static_cast<int>(input_.cursor()) * cw;
    // keep the cursor visible: scroll the label left when needed
    int offset = 0;
    if (cursor_x > visible) offset = cursor_x - visible;
    const int font_h = lv_font_get_line_height(ctx_.style.mono18);
    lv_obj_set_pos(input_label_, 6 - offset, (kInputH - 2 - font_h) / 2);
    lv_obj_set_pos(cursor_, 6 - offset + cursor_x - 1, 4);
}

void CalcPage::refresh_preview() {
    if (!preview_) return;
    const auto& s = ctx_.style;
    if (!message_.empty()) {
        lv_obj_set_style_text_color(preview_, s.error, 0);
        lv_label_set_text_fmt(preview_, "! %s", message_.c_str());
        return;
    }
    lv_obj_set_style_text_color(preview_, s.dim, 0);

    // name suggestions take the line while typing an identifier
    const auto suggestions = completer_.suggestions(input_, ctx_.engine.env);
    if (!suggestions.empty()) {
        const size_t current = completer_.cycling() ? completer_.index() : 0;
        char accent[8];
        lv_snprintf(accent, sizeof(accent), "%06x", static_cast<unsigned>(lv_color_to_u32(s.accent) & 0xffffff));
        std::string line = "SPC ";
        for (size_t i = 0; i < suggestions.size(); ++i) {
            const std::string name = suggestions[i].name + (suggestions[i].call ? "(" : "");
            line += i == current ? std::string("#") + accent + " " + name + "# " : name + " ";
        }
        lv_obj_set_style_text_align(preview_, LV_TEXT_ALIGN_LEFT, 0);
        lv_label_set_text(preview_, line.c_str());
        return;
    }
    lv_obj_set_style_text_align(preview_, LV_TEXT_ALIGN_RIGHT, 0);

    if (input_.empty()) {
        lv_label_set_text(preview_, "");
        return;
    }
    const auto r = ctx_.engine.preview(calc::prepare_input(input_.text()));
    if (r.ok && !r.text.empty() && r.text != input_.text()) {
        lv_label_set_text_fmt(preview_, "= %s", r.text.c_str());
    } else {
        lv_label_set_text(preview_, "");
    }
}

void CalcPage::refresh_footer() {
    if (!footer_) return;
    if (selected_ >= 0) {
        lv_label_set_text(footer_, "OK result  SPACE expr  Fn+F/X move  ESC back");
    } else {
        lv_label_set_text(footer_, "OK =  SPC complete  Fn+F history  TAB graph");
    }
}

bool CalcPage::on_key(const cz::KeyEvent& ev) {
    if (!ev.pressed_or_repeat()) return false;
    auto& hist = ctx_.engine.history;
    const uint32_t k = ev.key;

    // history browsing
    if (k == key::Up) {
        if (hist.empty()) return true;
        if (selected_ < 0) selected_ = static_cast<int>(hist.size()) - 1;
        else if (selected_ > first_rendered_) --selected_;
        refresh_selection();
        return true;
    }
    if (k == key::Down) {
        if (selected_ < 0) return true;
        ++selected_;
        if (selected_ >= static_cast<int>(hist.size())) selected_ = -1;
        refresh_selection();
        return true;
    }
    if (selected_ >= 0) {
        if (ev.phase != cz::KeyPhase::Press) return true;
        const auto& h = hist[static_cast<size_t>(selected_)];
        if (k == key::Enter) {
            // the result value only (strip "name = " of assignments)
            std::string out = h.output;
            if (const auto eq = out.find(" = "); eq != std::string::npos) out = out.substr(eq + 3);
            if (out.find("defined") == std::string::npos) insert(out);
            else insert(h.input.substr(0, h.input.find('(')));
            return true;
        }
        if (k == ' ') {
            insert(h.input);
            return true;
        }
        if (k == key::Esc) {
            selected_ = -1;
            refresh_selection();
            return true;
        }
        // any other key: back to editing and handle it there
        selected_ = -1;
        refresh_selection();
    }

    switch (k) {
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
            if (input_.empty() && message_.empty()) return false;  // let hold-Esc quit
            input_.clear();
            break;
        default:
            if (!platform::key::is_printable(k)) return false;
            if (k == ' ') {
                // Space completes names; a plain space only when there is nothing to complete
                if (!completer_.accept(input_, ctx_.engine.env)) input_.insert(' ');
                message_.clear();
                refresh_input();
                refresh_preview();
                return true;
            }
            input_.type(static_cast<char>(k));
            break;
    }
    completer_.reset();
    message_.clear();
    refresh_input();
    refresh_preview();
    return true;
}

}  // namespace calc_ui
