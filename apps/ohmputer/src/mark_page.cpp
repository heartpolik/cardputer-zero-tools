#include "mark_page.h"

#include "eseries.h"
#include "resistor_view.h"

namespace ohm_ui {
namespace {

namespace key = platform::key;

constexpr int kInputY = 2;
constexpr int kInputH = 24;
constexpr int kResultsY = 30;
constexpr int kRowH = 30;

const char* kind_label(char k) {
    switch (k) {
        case 'R': return "R";
        case 'C': return "C";
        case 'L': return "L";
        case 'D': return "D";
        case 'Q': return "Q";
        default: return "?";
    }
}

}  // namespace

void MarkPage::build(lv_obj_t* parent) {
    const auto& s = ctx_.style;
    parent_ = parent;
    input_.set(ctx_.session.mark_input);

    input_box_ = make_box(parent, 6, kInputY, kWidth - 12, kInputH);
    lv_obj_set_style_bg_color(input_box_, s.surface, 0);
    lv_obj_set_style_bg_opa(input_box_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(input_box_, 1, 0);
    lv_obj_set_style_border_color(input_box_, s.accent, 0);
    lv_obj_set_style_radius(input_box_, 4, 0);
    lv_obj_set_style_clip_corner(input_box_, true, 0);
    input_label_ = make_label(input_box_, s.mono18, s.text);
    lv_label_set_recolor(input_label_, true);
    cursor_ = make_box(input_box_, 0, 4, 2, kInputH - 10);
    lv_obj_set_style_bg_color(cursor_, s.accent, 0);
    lv_obj_set_style_bg_opa(cursor_, LV_OPA_COVER, 0);

    results_ = make_box(parent, 0, kResultsY, kWidth, kContentH - kResultsY);
    refresh();
}

void MarkPage::refresh() {
    if (!results_) return;
    const auto& s = ctx_.style;

    // input line (placeholder when empty)
    const int font_h = lv_font_get_line_height(s.mono18);
    if (input_.empty()) {
        char dim[8];
        lv_snprintf(dim, sizeof dim, "%06x", static_cast<unsigned>(lv_color_to_u32(s.dim) & 0xffffff));
        lv_label_set_text_fmt(input_label_, "#%s 103 4R7 01C 104K 2A104J M7#", dim);
    } else {
        lv_label_set_text(input_label_, input_.text().c_str());
    }
    const int cw = glyph_width(s.mono18);
    lv_obj_set_pos(input_label_, 6, (kInputH - 2 - font_h) / 2);
    lv_obj_set_pos(cursor_, 6 + static_cast<int>(input_.cursor()) * cw - 1, 4);

    lv_obj_clean(results_);
    if (input_.empty()) {
        auto* hint = make_label(results_, s.mono12, s.dim,
                                "Type the marking printed on a part:\n"
                                " SMD resistors  103  4992  4R7  01C\n"
                                " capacitors  104  4n7  104K  2A104J\n"
                                " tantalum  A106    inductors  101  4R7\n"
                                " diodes / transistors  M7  T4  1AM\n"
                                "Colour bands: Tab -> BANDS");
        lv_obj_set_style_text_line_space(hint, 2, 0);
        lv_obj_set_pos(hint, 8, 4);
        return;
    }
    const auto readings = ohm::decode_marking(input_.text());
    if (readings.empty()) {
        auto* l = make_label(results_, s.mono14, s.error, "unknown marking");
        lv_obj_set_pos(l, 8, 6);
        return;
    }
    int y = 0;
    for (size_t i = 0; i < readings.size() && static_cast<int>(i) < kRows; ++i, y += kRowH) {
        const auto& r = readings[i];
        const lv_color_t kc = kind_color(s, r.kind);
        auto* badge = make_box(results_, 8, y + 3, 18, 18);
        lv_obj_set_style_border_color(badge, kc, 0);
        lv_obj_set_style_border_width(badge, 1, 0);
        lv_obj_set_style_radius(badge, 3, 0);
        auto* k = make_label(badge, s.ui12, kc, kind_label(r.kind));
        lv_obj_center(k);

        auto* v = make_label(results_, s.mono14, s.text, r.value.c_str());
        lv_obj_set_pos(v, 34, y);
        auto* how = make_label(results_, s.ui10, s.dim, r.how.c_str());
        lv_obj_set_pos(how, 34, y + 17);

        // resistors: the same value as colour bands, and whether it is a preferred value
        if (r.kind == 'R' && r.number > 0) {
            const auto bands = ohm::encode_bands(r.number, 2, 0.05);
            const auto five = bands.empty() ? ohm::encode_bands(r.number, 3, 0.01) : bands;
            if (!five.empty()) draw_resistor(results_, s, kWidth - 96, y + 15, 64, 12, five);
            const auto series = ctx_.session.series;
            if (ohm::in_series(r.number, series)) {
                auto* e = make_label(results_, s.ui10, s.dim, ohm::series_name(series));
                lv_obj_align(e, LV_ALIGN_TOP_RIGHT, -8, y + 2);
            }
        }
    }
    if (static_cast<int>(readings.size()) > kRows) {
        auto* more = make_label(results_, s.ui10, s.dim);
        lv_label_set_text_fmt(more, "+%d more", static_cast<int>(readings.size()) - kRows);
        lv_obj_align(more, LV_ALIGN_BOTTOM_RIGHT, -8, 0);
    }
}

bool MarkPage::on_key(const cz::KeyEvent& ev) {
    if (!ev.pressed_or_repeat()) return false;
    switch (ev.key) {
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
        case key::Enter:
            return true;
        default:
            if (!platform::key::is_printable(ev.key) || input_.text().size() >= 16) return false;
            input_.insert(static_cast<char>(ev.key));
            break;
    }
    ctx_.session.mark_input = input_.text();
    ctx_.mark_dirty();
    refresh();
    return true;
}

}  // namespace ohm_ui
