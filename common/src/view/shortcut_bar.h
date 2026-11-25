// Labels for the number keys 4..8, which sit right under the CardputerZero display.
//
// Same layout as the official CardputerZero Store (appstore_shortcut_bar.cpp): one label
// per key along the bottom edge, centred over the key (x = 48, 104, 160, 216, 272) and
// coloured per key (4 yellow, 5 blue, 6 green, 7 purple, 8 orange). By the Store's
// convention 4 is "back" and 8 the main action. A disabled action is drawn grey.
#pragma once

#include "lvgl.h"
#include "ui_style.h"

namespace cz {

class ShortcutBar {
public:
    static constexpr int kFirstKey = 4;
    static constexpr int kLastKey = 8;
    static constexpr int kHeight = 17;  // the bar occupies y = 153..169

    static int center_x(int key) { return 48 + (key - kFirstKey) * 56; }
    static lv_color_t key_color(int key);
    // the digit typed on that key, for on_key: is_key(ev.key) / key_of(ev.key)
    static bool is_key(uint32_t code) { return code >= '0' + kFirstKey && code <= '0' + kLastKey; }
    static int key_of(uint32_t code) { return static_cast<int>(code - '0'); }

    // y: top of the bar inside parent (default: the screen's bottom edge for a full-screen parent;
    // a page box that starts under the status bar passes kHeight - kStatusHeight - ShortcutBar::kHeight)
    void build(lv_obj_t* parent, const Style& style, int y = cz::kHeight - kHeight);
    // text empty: nothing shown for that key
    void set(int key, const char* text, bool enabled = true);
    void clear();
    lv_obj_t* root() const { return root_; }

private:
    lv_obj_t* root_{nullptr};
    lv_obj_t* labels_[kLastKey - kFirstKey + 1]{};
    lv_color_t dim_{};
};

}  // namespace cz
