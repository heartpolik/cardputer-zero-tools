#include "shortcut_bar.h"

namespace cz {
namespace {

constexpr int kLabelW = 56;

}  // namespace

lv_color_t ShortcutBar::key_color(int key) {
    switch (key) {
        case 4: return lv_color_hex(0xF0B429);
        case 5: return lv_color_hex(0x168CE5);
        case 6: return lv_color_hex(0x7ED957);
        case 7: return lv_color_hex(0xB069FF);
        default: return lv_color_hex(0xFF8A2A);
    }
}

void ShortcutBar::build(lv_obj_t* parent, const Style& style, int y) {
    dim_ = style.dim;
    root_ = make_box(parent, 0, y, kWidth, kHeight);
    for (int k = kFirstKey; k <= kLastKey; ++k) {
        auto* l = make_label(root_, style.ui12, key_color(k));
        lv_obj_set_pos(l, center_x(k) - kLabelW / 2, 1);
        lv_obj_set_size(l, kLabelW, kHeight - 1);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
        labels_[k - kFirstKey] = l;
    }
}

void ShortcutBar::set(int key, const char* text, bool enabled) {
    if (key < kFirstKey || key > kLastKey || !root_) return;
    auto* l = labels_[key - kFirstKey];
    lv_label_set_text(l, text ? text : "");
    lv_obj_set_style_text_color(l, enabled ? key_color(key) : dim_, 0);
}

void ShortcutBar::clear() {
    for (int k = kFirstKey; k <= kLastKey; ++k) set(k, "");
}

}  // namespace cz
