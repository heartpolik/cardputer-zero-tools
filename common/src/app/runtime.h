// Shared application runtime for cardputer-zero-tools apps.
//
// Owns LVGL init, the display (fbdev on device, SDL simulator on desktop),
// keyboard routing, theme (dark/light, persisted per app), toast overlay and
// the main loop. Apps create a Runtime, build their screens on lv_screen_active()
// and register a key handler.
//
// Scripted runs (demos, screenshots, smoke tests on the device):
//   CZ_SCRIPT='2+2{enter}{tab}{shot:/tmp/a.png}{wait:500}{quit}'
//   CZ_ASSETS=/path/to/share/<app>   (asset root override)
// tokens: {enter} {tab} {stab} {esc} {bs} {del} {up} {down} {left} {right}
//         {home} {end} {help} {f1}..{f12} {wait:ms} {shot:path} {quit}  ("{{" = '{')
//         {layer:fn} {layer:sym!} {layer:shift} {layer:base}  pretend a modifier layer (! = locked)
//
// Global keys handled here unless the app consumes them first:
//   PrintScreen (Fn+J)  screenshot
//   hold Esc            quit
#pragma once

#include "asset_manager.h"
#include "linux_input.h"
#include "modifier_watch.h"
#include "lvgl.h"
#include "toast.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace cz {

using platform::KeyLayer;
using platform::KeyPhase;
using platform::LayerState;

struct KeyEvent {
    uint32_t key;
    KeyPhase phase;
    platform::Modifiers mods;

    bool pressed_or_repeat() const { return phase == KeyPhase::Press || phase == KeyPhase::Repeat; }
};

class Runtime {
public:
    // dev_assets_root: the app's source assets/ dir (desktop runs); installed
    // builds also look in /usr/share/<app_name>/.
    explicit Runtime(std::string app_name, std::filesystem::path dev_assets_root = {});
    ~Runtime();
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;

    bool init();
    int run();
    void quit();

    app::AssetManager& assets() { return *assets_; }
    lv_display_t* display() const { return display_; }
    view::widgets::Toast& toast() { return *toast_; }

    bool dark_mode() const;
    void set_dark_mode(bool enabled);
    lv_subject_t* dark_mode_subject() { return &dark_mode_; }

    // Return true when handled. Unhandled keys fall through to runtime globals.
    void set_key_handler(std::function<bool(const KeyEvent&)> handler) { handler_ = std::move(handler); }

    // The layer the next key lands in: Fn / Sym / Shift tapped (once) or locked on the
    // built-in keyboard. The handler runs on the UI thread when it changes.
    LayerState key_layer() const { return layer_; }
    void set_layer_handler(std::function<void(LayerState)> handler) { layer_handler_ = std::move(handler); }

    // ~/.config/<app>/ (created on demand); app state files live here.
    std::filesystem::path state_dir() const;

    const std::string& app_name() const { return app_name_; }

private:
    static bool key_listener(uint32_t key, KeyPhase phase, void* self);
    bool on_key(uint32_t key, KeyPhase phase);
    void load_config();
    void start_script(const char* script);
    void script_step();
    void save_config();

    std::string app_name_;
    std::filesystem::path dev_assets_root_;
    std::unique_ptr<app::AssetManager> assets_;
    lv_display_t* display_{nullptr};
    std::unique_ptr<view::widgets::Toast> toast_;
    lv_subject_t dark_mode_{};
    std::function<bool(const KeyEvent&)> handler_;
    std::function<void(LayerState)> layer_handler_;
    LayerState layer_;
    void poll_layer();
    bool running_{false};
    std::string script_;
    size_t script_pos_{0};
    uint32_t script_wait_until_{0};
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace cz
