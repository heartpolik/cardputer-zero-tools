#include "bands_page.h"

#include "eseries.h"
#include "resistor_view.h"
#include "si.h"

namespace ohm_ui {
namespace {

namespace key = platform::key;
using ohm::Color;

const char* const kOhm = "\xCE\xA9";

std::string tolerance_text(double t) { return "\xC2\xB1" + ohm::format_plain(t * 100, 3) + " %"; }

// legend: digit keys and the colour they stand for
void draw_legend(lv_obj_t* parent, const Style& s, int y) {
    const char keys[] = "0123456789GS";
    const int w = 25, x0 = (kWidth - 12 * w) / 2;
    for (int i = 0; i < 12; ++i) {
        const auto c = *ohm::color_from_key(keys[i]);
        auto* sw = make_box(parent, x0 + i * w + 2, y, w - 4, 12);
        lv_obj_set_style_bg_color(sw, lv_color_hex(ohm::color_rgb(c)), 0);
        lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(sw, 2, 0);
        lv_obj_set_style_border_color(sw, s.border, 0);
        lv_obj_set_style_border_width(sw, 1, 0);
        const char text[2] = {keys[i], 0};
        auto* l = make_label(parent, s.ui10, s.dim, text);
        lv_obj_set_pos(l, x0 + i * w + w / 2 - 3, y + 13);
    }
}

}  // namespace

const char* BandsPage::status() const { return ctx_.session.band_from_value ? "VALUE > BANDS" : "BANDS > VALUE"; }

void BandsPage::build(lv_obj_t* parent) {
    value_.set(ctx_.session.band_value);
    area_ = make_box(parent, 0, 0, kWidth, kContentH);
    refresh();
}

void BandsPage::refresh() {
    if (!area_) return;
    lv_obj_clean(area_);
    if (ctx_.session.band_from_value) refresh_value_mode();
    else refresh_colors_mode();
    ctx_.status_changed();
}

void BandsPage::refresh_colors_mode() {
    const auto& s = ctx_.style;
    const auto& bands = ctx_.session.bands;

    draw_resistor(area_, s, 40, 8, 200, 40, bands);

    // band names under the drawing
    std::string names;
    for (size_t i = 0; i < bands.size(); ++i) names += std::string(i ? " " : "") + ohm::color_short(bands[i]);
    auto* nl = make_label(area_, s.mono12, s.dim, bands.empty() ? "type the bands from the left: 4 7 2 G" : names.c_str());
    lv_obj_align(nl, LV_ALIGN_TOP_MID, 0, 52);

    auto* value = make_label(area_, s.mono_bold18, s.accent);
    lv_obj_align(value, LV_ALIGN_TOP_MID, 0, 70);
    auto* detail = make_label(area_, s.mono12, s.dim);
    lv_obj_align(detail, LV_ALIGN_TOP_MID, 0, 94);

    if (!bands.empty()) {
        const auto r = ohm::decode_bands(bands);
        if (r.ok) {
            const std::string v = ohm::format_si(r.ohms, kOhm, 4) + "  " + tolerance_text(r.tolerance);
            lv_label_set_text(value, v.c_str());
            std::string d;
            if (r.tempco) d += std::to_string(r.tempco) + " ppm/K   ";
            const auto series = ctx_.session.series;
            if (r.ohms > 0) {
                d += ohm::in_series(r.ohms, series) ? std::string(ohm::series_name(series)) + " value"
                                                    : std::string("not in ") + ohm::series_name(series);
            }
            if (r.reversed) d += "   (read from the other end)";
            lv_label_set_text(detail, d.c_str());
        } else {
            lv_obj_set_style_text_color(value, s.error, 0);
            lv_label_set_text(value, bands.size() < 3 ? "..." : "?");
            lv_label_set_text(detail, bands.size() < 3 ? "3 to 6 bands" : r.error.c_str());
        }
    }
    draw_legend(area_, s, 112);
    auto* help = make_label(area_, s.ui10, s.dim, "BS undo  ESC clear  SPACE value > bands");
    lv_obj_align(help, LV_ALIGN_BOTTOM_MID, 0, -1);
}

void BandsPage::refresh_value_mode() {
    const auto& s = ctx_.style;

    auto* box = make_box(area_, 6, 2, kWidth - 12, 24);
    lv_obj_set_style_bg_color(box, s.surface, 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_border_color(box, s.accent, 0);
    lv_obj_set_style_radius(box, 4, 0);
    auto* text = make_label(box, s.mono18, s.text, value_.empty() ? "" : value_.text().c_str());
    lv_obj_set_pos(text, 6, 1);
    auto* cursor = make_box(box, 6 + static_cast<int>(value_.text().size()) * glyph_width(s.mono18) - 1, 4, 2, 14);
    lv_obj_set_style_bg_color(cursor, s.accent, 0);
    lv_obj_set_style_bg_opa(cursor, LV_OPA_COVER, 0);
    auto* unit = make_label(box, s.mono14, s.dim, kOhm);
    lv_obj_align(unit, LV_ALIGN_RIGHT_MID, -6, 0);

    if (value_.empty()) {
        auto* hint = make_label(area_, s.mono12, s.dim, "a value: 4k7  220  1M  10R  4.99k\n\nSPACE: back to bands > value");
        lv_obj_set_pos(hint, 10, 36);
        return;
    }
    const auto v = ohm::parse_si(value_.text());
    if (!v || *v <= 0) {
        auto* e = make_label(area_, s.mono14, s.error, "can't read that value");
        lv_obj_set_pos(e, 10, 36);
        return;
    }
    const auto series = ctx_.session.series;
    const double tol = ohm::series_tolerance(series) > 0.05 ? 0.05 : ohm::series_tolerance(series);
    int y = 34;
    for (int digits : {2, 3}) {
        const auto bands = ohm::encode_bands(*v, digits, digits == 2 ? (tol < 0.05 ? 0.05 : tol) : 0.01);
        auto* label = make_label(area_, s.ui10, s.dim, digits == 2 ? "4 BANDS" : "5 BANDS");
        lv_obj_set_pos(label, 8, y + 4);
        if (bands.empty()) {
            auto* n = make_label(area_, s.mono12, s.dim, "needs more digits");
            lv_obj_set_pos(n, 70, y + 4);
        } else {
            draw_resistor(area_, s, 70, y, 110, 18, bands);
            std::string names;
            for (size_t i = 0; i < bands.size(); ++i) names += std::string(i ? " " : "") + ohm::color_short(bands[i]);
            auto* n = make_label(area_, s.ui10, s.text, names.c_str());
            lv_obj_set_pos(n, 88, y + 19);
        }
        y += 33;
    }
    const double near = ohm::nearest(*v, series);
    std::string d = ohm::format_si(*v, kOhm, 4);
    d += ohm::in_series(*v, series) ? std::string("  is an ") + ohm::series_name(series) + " value"
                                    : std::string("  nearest ") + ohm::series_name(series) + ": " +
                                          ohm::format_si(near, kOhm, 4);
    auto* info = make_label(area_, s.mono12, s.accent, d.c_str());
    lv_obj_set_pos(info, 8, y + 4);
    auto* help = make_label(area_, s.ui10, s.dim, "F2 series   SPACE bands > value");
    lv_obj_align(help, LV_ALIGN_BOTTOM_MID, 0, -1);
}

bool BandsPage::on_key(const cz::KeyEvent& ev) {
    if (!ev.pressed_or_repeat()) return false;
    auto& session = ctx_.session;
    const uint32_t k = ev.key;
    if (k == ' ') {
        if (ev.phase != cz::KeyPhase::Press) return true;
        session.band_from_value = !session.band_from_value;
    } else if (session.band_from_value) {
        if (k == key::Backspace) value_.backspace();
        else if (k == key::Esc) {
            if (ev.phase != cz::KeyPhase::Press) return true;
            if (value_.empty()) return false;
            value_.clear();
        } else if (platform::key::is_printable(k) && value_.text().size() < 12) {
            value_.insert(static_cast<char>(k));
        } else {
            return false;
        }
        session.band_value = value_.text();
    } else {
        auto& bands = session.bands;
        if (k == key::Backspace) {
            if (!bands.empty()) bands.pop_back();
        } else if (k == key::Esc) {
            if (ev.phase != cz::KeyPhase::Press) return true;
            if (bands.empty()) return false;
            bands.clear();
        } else if (platform::key::is_printable(k)) {
            const auto c = ohm::color_from_key(static_cast<char>(k));
            if (!c || ev.phase != cz::KeyPhase::Press) return true;
            if (bands.size() >= 6) bands.clear();
            bands.push_back(*c);
        } else {
            return false;
        }
    }
    ctx_.mark_dirty();
    refresh();
    return true;
}

}  // namespace ohm_ui
