#include "resistor_view.h"

namespace ohm_ui {

lv_color_t kind_color(const Style& s, char kind) {
    switch (kind) {
        case 'R': return s.accent;
        case 'C': return s.plot[1];
        case 'L': return s.warn;
        default: return s.plot[3];
    }
}

lv_obj_t* draw_resistor(lv_obj_t* parent, const Style& s, int x, int y, int body_w, int body_h,
                        const std::vector<ohm::Color>& bands, bool with_leads) {
    const int lead = with_leads ? body_h : 0;
    auto* box = make_box(parent, x, y, body_w + 2 * lead, body_h);

    if (with_leads) {
        for (int side = 0; side < 2; ++side) {
            auto* l = make_box(box, side == 0 ? 0 : body_w + lead, body_h / 2 - 1, lead, 2);
            lv_obj_set_style_bg_color(l, s.axis, 0);
            lv_obj_set_style_bg_opa(l, LV_OPA_COVER, 0);
        }
    }
    auto* body = make_box(box, lead, 0, body_w, body_h);
    lv_obj_set_style_bg_color(body, lv_color_hex(0xd8c39a), 0);
    lv_obj_set_style_bg_opa(body, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(body, body_h / 3, 0);
    lv_obj_set_style_clip_corner(body, true, 0);

    // digit bands spread from the left, the tolerance (and tempco) band at the right end
    const size_t n = bands.size();
    if (n == 0) return box;
    const int band_w = body_w / 14 > 3 ? body_w / 14 : 3;
    const int step = band_w * 2;
    const size_t right = n >= 4 ? (n == 6 ? 2 : 1) : 0;  // bands drawn at the right end
    auto place = [&](size_t i, int bx) {
        auto* b = make_box(body, bx, 0, band_w, body_h);
        lv_obj_set_style_bg_color(b, lv_color_hex(ohm::color_rgb(bands[i])), 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        if (bands[i] == ohm::Color::Black || bands[i] == ohm::Color::White) {
            lv_obj_set_style_border_color(b, lv_color_hex(0x5a5040), 0);
            lv_obj_set_style_border_width(b, 1, 0);
            lv_obj_set_style_border_side(b, static_cast<lv_border_side_t>(LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_RIGHT), 0);
        }
    };
    int bx = body_w / 7;
    for (size_t i = 0; i < n - right; ++i, bx += step) place(i, bx);
    int rx = body_w - body_w / 7 - band_w;
    for (size_t k = 0; k < right; ++k, rx -= step) place(n - 1 - k, rx);
    return box;
}

}  // namespace ohm_ui
