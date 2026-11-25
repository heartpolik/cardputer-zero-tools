#include "shortcut_bar.h"

namespace cz {
namespace {

constexpr int kLabelW = 54;
constexpr int kTagW = 19;

int layer_index(platform::KeyLayer l) { return static_cast<int>(l); }

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

const char* ShortcutBar::typed(int key, platform::KeyLayer layer) {
    // from the device keymaps (linux,keymap / keymap1 / keymap2) and the US shift row
    static const char* shift[kKeys] = {"$", "%", "^", "&", "*"};
    static const char* sym[kKeys] = {"'", "`", "\\", ",", "."};
    static const char* fn[kKeys] = {"F4", "F5", "F6", "F7", "F8"};
    static const char* base[kKeys] = {"4", "5", "6", "7", "8"};
    if (key < kFirstKey || key > kLastKey) return "";
    const int i = key - kFirstKey;
    switch (layer) {
        case platform::KeyLayer::Shift: return shift[i];
        case platform::KeyLayer::Sym: return sym[i];
        case platform::KeyLayer::Fn: return fn[i];
        default: return base[i];
    }
}

void ShortcutBar::build(lv_obj_t* parent, const Style& style, int y) {
    dim_ = style.dim;
    bg_ = style.bg;
    text_ = style.text;
    accent_ = style.accent;
    root_ = make_box(parent, 0, y, kWidth, kHeight);
    lv_obj_set_style_bg_color(root_, style.bg, 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);

    tag_ = make_label(root_, style.ui10, style.accent);
    lv_obj_set_pos(tag_, 1, 2);
    lv_obj_set_size(tag_, kTagW, kHeight - 4);
    lv_obj_set_style_text_align(tag_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_radius(tag_, 3, 0);
    lv_label_set_long_mode(tag_, LV_LABEL_LONG_MODE_CLIP);

    for (int k = kFirstKey; k <= kLastKey; ++k) {
        auto* l = make_label(root_, style.ui12, key_color(k));
        lv_obj_set_pos(l, center_x(k) - kLabelW / 2, 1);
        lv_obj_set_size(l, kLabelW, kHeight - 1);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
        labels_[k - kFirstKey] = l;
    }
    render();
}

void ShortcutBar::set(int key, const char* text, bool enabled, platform::KeyLayer layer) {
    if (key < kFirstKey || key > kLastKey) return;
    Slot& s = slots_[layer_index(layer)][key - kFirstKey];
    s.text = text ? text : "";
    s.enabled = enabled;
    s.set = true;
    if (layer == state_.layer) render();
}

void ShortcutBar::clear(platform::KeyLayer layer) {
    for (auto& s : slots_[layer_index(layer)]) s = Slot{};
    if (layer == state_.layer) render();
}

void ShortcutBar::show_layer(platform::LayerState state) {
    state_ = state;
    render();
}

void ShortcutBar::set_auto_hide(bool on) {
    auto_hide_ = on;
    render();
}

void ShortcutBar::render() {
    if (!root_) return;
    const bool base = state_.layer == platform::KeyLayer::Base;
    if (auto_hide_ && base) {
        lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);

    // the layer tag: outlined while armed for one key, filled when locked
    if (base) {
        lv_label_set_text(tag_, "");
        lv_obj_set_style_bg_opa(tag_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(tag_, 0, 0);
    } else {
        const char* name = state_.layer == platform::KeyLayer::Fn    ? "Fn"
                           : state_.layer == platform::KeyLayer::Sym ? "Sy"
                                                                     : "Sh";
        lv_label_set_text(tag_, name);
        lv_obj_set_style_border_color(tag_, accent_, 0);
        lv_obj_set_style_border_width(tag_, 1, 0);
        lv_obj_set_style_bg_color(tag_, accent_, 0);
        lv_obj_set_style_bg_opa(tag_, state_.locked ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(tag_, state_.locked ? bg_ : accent_, 0);
    }

    for (int k = kFirstKey; k <= kLastKey; ++k) {
        const Slot& s = slots_[layer_index(state_.layer)][k - kFirstKey];
        auto* l = labels_[k - kFirstKey];
        if (s.set) {
            lv_label_set_text(l, s.text.c_str());
            lv_obj_set_style_text_color(l, s.enabled ? key_color(k) : dim_, 0);
        } else if (!base) {
            // no action for it here: show what the key types in this layer
            lv_label_set_text(l, typed(k, state_.layer));
            lv_obj_set_style_text_color(l, dim_, 0);
        } else {
            lv_label_set_text(l, "");
        }
    }
}

}  // namespace cz
