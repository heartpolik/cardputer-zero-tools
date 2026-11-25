// Labels for the number keys 4..8, which sit right under the CardputerZero display.
//
// Same layout as the official CardputerZero Store (appstore_shortcut_bar.cpp): one label
// per key along the bottom edge, centred over the key (x = 48, 104, 160, 216, 272) and
// coloured per key (4 yellow, 5 blue, 6 green, 7 purple, 8 orange). By the Store's
// convention 4 is "back" and 8 the main action. A disabled action is drawn grey.
//
// Layers: while Shift, Sym or Fn is tapped or locked (Runtime::key_layer()), the bar shows
// what 4..8 do in that layer: the app's labels for it, or else what the key types
// ($ % ^ & *, ' ` \ , ., F4..F8) in grey. A tag on the left names the layer (filled
// when locked). With auto_hide the bar only appears while a modifier is active, for
// screens where 4..8 are plain digits (calculators).
#pragma once

#include "lvgl.h"
#include "modifier_watch.h"
#include "ui_style.h"

#include <string>

namespace cz {

class ShortcutBar {
public:
    static constexpr int kFirstKey = 4;
    static constexpr int kLastKey = 8;
    static constexpr int kHeight = 17;  // full-screen parent: y = 153..169
    static constexpr int kKeys = kLastKey - kFirstKey + 1;

    static int center_x(int key) { return 48 + (key - kFirstKey) * 56; }
    static lv_color_t key_color(int key);
    // the digit typed on that key, for on_key: is_key(ev.key) / key_of(ev.key)
    static bool is_key(uint32_t code) { return code >= '0' + kFirstKey && code <= '0' + kLastKey; }
    static int key_of(uint32_t code) { return static_cast<int>(code - '0'); }
    // what key 4..8 types in a layer: "$", "'", "F4"
    static const char* typed(int key, platform::KeyLayer layer);

    // y: top of the bar inside parent (default: the screen's bottom edge for a full-screen parent;
    // a page box that starts under the status bar passes kContentH - ShortcutBar::kHeight)
    void build(lv_obj_t* parent, const Style& style, int y = cz::kHeight - kHeight);
    // text empty: nothing shown for that key in that layer
    void set(int key, const char* text, bool enabled = true, platform::KeyLayer layer = platform::KeyLayer::Base);
    void clear(platform::KeyLayer layer = platform::KeyLayer::Base);
    // show the given layer (call from Runtime::set_layer_handler)
    void show_layer(platform::LayerState state);
    // only visible while a modifier layer is active
    void set_auto_hide(bool on);
    lv_obj_t* root() const { return root_; }

private:
    struct Slot {
        std::string text;
        bool enabled{true};
        bool set{false};
    };
    void render();

    lv_obj_t* root_{nullptr};
    lv_obj_t* tag_{nullptr};
    lv_obj_t* labels_[kKeys]{};
    Slot slots_[4][kKeys];
    platform::LayerState state_;
    bool auto_hide_{false};
    lv_color_t dim_{}, bg_{}, text_{}, accent_{};
};

}  // namespace cz
