#include "runtime.h"

#include "application_config.h"
#include "logger.h"
#include "screenshot_service.h"
#include "theme.h"

#include <cctype>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <utility>
#include <system_error>

#if USE_DESKTOP
#include "desktop_simulator_frame.h"
#else
#if APP_USE_DRM
#include "src/drivers/display/drm/lv_linux_drm.h"
#else
#include "src/drivers/display/fb/lv_linux_fbdev.h"
#endif
#endif

#ifndef APP_FRAMEBUFFER_DEVICE
#define APP_FRAMEBUFFER_DEVICE "/dev/fb0"
#endif
#ifndef APP_DRM_DEVICE
#define APP_DRM_DEVICE "/dev/dri/card0"
#endif
#ifndef APP_DRM_CONNECTOR_ID
#define APP_DRM_CONNECTOR_ID -1
#endif

namespace cz {

struct Runtime::Impl {
#if USE_DESKTOP
    std::unique_ptr<app::DesktopSimulatorFrame> frame;
#endif
    lv_observer_t* theme_observer{nullptr};
};

namespace {

#if !USE_DESKTOP
lv_display_t* create_device_display() {
#if APP_USE_DRM
    auto* display = lv_linux_drm_create();
    if (display && lv_linux_drm_set_file(display, APP_DRM_DEVICE, APP_DRM_CONNECTOR_ID) != LV_RESULT_OK) {
        lv_display_delete(display);
        return nullptr;
    }
    return display;
#else
    auto* display = lv_linux_fbdev_create();
    if (display && lv_linux_fbdev_set_file(display, APP_FRAMEBUFFER_DEVICE) != LV_RESULT_OK) {
        lv_display_delete(display);
        return nullptr;
    }
    return display;
#endif
}
#endif

}  // namespace

Runtime::Runtime(std::string app_name, std::filesystem::path dev_assets_root)
    : app_name_(std::move(app_name)), dev_assets_root_(std::move(dev_assets_root)), impl_(std::make_unique<Impl>()) {}

Runtime::~Runtime() {
    platform::clear_key_listener(&Runtime::key_listener, this);
    toast_.reset();
    if (impl_->theme_observer) lv_observer_remove(impl_->theme_observer);
}

std::filesystem::path Runtime::state_dir() const {
    std::filesystem::path root;
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && xdg[0] == '/') {
        root = xdg;
    } else if (const char* home = std::getenv("HOME"); home && home[0] == '/') {
        root = std::filesystem::path(home) / ".config";
    } else {
        root = "/tmp";
    }
    auto dir = root / app_name_;
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

void Runtime::load_config() {
    app::ApplicationConfig config;
    config.dark_mode = true;  // default for our tools
    std::string error;
    const auto path = (state_dir() / "settings.conf").string();
    if (!app::load_application_config(path, config, error)) {
        config.dark_mode = true;
    }
    lv_subject_set_int(&dark_mode_, config.dark_mode ? 1 : 0);
}

void Runtime::save_config() {
    app::ApplicationConfig config;
    config.dark_mode = dark_mode();
    std::string error;
    const auto path = (state_dir() / "settings.conf").string();
    if (!app::save_application_config(path, config, error)) {
        LOG_WARN("failed to save {}: {}", path, error);
    }
}

bool Runtime::init() {
    logger::Logger::init();
    logger::Logger::set_tag(app_name_.c_str());
    lv_init();

    lv_subject_init_int(&dark_mode_, 1);
    load_config();

    // CZ_ASSETS overrides the asset root (run an unpacked build on the device)
    std::filesystem::path assets_root = dev_assets_root_;
    if (const char* env = std::getenv("CZ_ASSETS"); env && env[0]) assets_root = env;
    assets_ = std::make_unique<app::AssetManager>(app_name_, assets_root);

#if USE_DESKTOP
    impl_->frame = std::make_unique<app::DesktopSimulatorFrame>(*assets_);
    display_ = impl_->frame->display();
    if (display_) impl_->frame->bind_dark_mode(&dark_mode_);
#else
    display_ = create_device_display();
    if (display_) platform::init_key_input(display_);
#endif
    if (!display_) {
        LOG_ERROR("failed to initialize display");
        return false;
    }

    auto* font = assets_->load_standard_font(14);
    view::apply_lvgl_theme(display_, dark_mode(), font ? font : LV_FONT_DEFAULT);
    impl_->theme_observer = lv_subject_add_observer(
        &dark_mode_,
        [](lv_observer_t* observer, lv_subject_t* subject) {
            auto* self = static_cast<Runtime*>(lv_observer_get_user_data(observer));
            auto* f = self->assets_->load_standard_font(14);
            view::apply_lvgl_theme(self->display_, lv_subject_get_int(subject) != 0, f ? f : LV_FONT_DEFAULT);
        },
        this);

    toast_ = std::make_unique<view::widgets::Toast>(lv_layer_top(), &dark_mode_, *assets_);
    toast_->build();

    platform::set_key_listener(&Runtime::key_listener, this);
    if (const char* script = std::getenv("CZ_SCRIPT"); script && script[0]) start_script(script);
    LOG_INFO("{} started at {}x{}", app_name_, lv_display_get_horizontal_resolution(display_),
             lv_display_get_vertical_resolution(display_));
    return true;
}

bool Runtime::dark_mode() const { return lv_subject_get_int(const_cast<lv_subject_t*>(&dark_mode_)) != 0; }

void Runtime::set_dark_mode(bool enabled) {
    if (enabled == dark_mode()) return;
    lv_subject_set_int(&dark_mode_, enabled ? 1 : 0);
    save_config();
}

void Runtime::quit() { running_ = false; }

bool Runtime::key_listener(uint32_t key, KeyPhase phase, void* self) {
    return static_cast<Runtime*>(self)->on_key(key, phase);
}

bool Runtime::on_key(uint32_t key, KeyPhase phase) {
    const KeyEvent event{key, phase, platform::current_modifiers()};

    if (key == platform::key::PrintScreen) {
        if (phase == KeyPhase::Press) {
            const auto result = platform::screenshot::capture_active_screen();
            if (result.success) {
                toast_->show_highlighted("Saved ", result.path, "", view::widgets::ToastTone::Success, 4000, true);
            } else {
                toast_->show("Screenshot failed: " + result.error, view::widgets::ToastTone::Error, 3000);
            }
        }
        return true;
    }

    // hold Esc to quit (the app still sees the short press)
    if (key == platform::key::Esc) {
        if (phase == KeyPhase::LongPress) {
            toast_->hide();
            quit();
            return true;
        }
    }

    if (handler_ && handler_(event)) return true;
    return key == platform::key::Esc;
}

void Runtime::start_script(const char* script) {
    script_ = script;
    script_pos_ = 0;
    script_wait_until_ = lv_tick_get() + 300;  // let the first frame settle
    lv_timer_create(
        [](lv_timer_t* t) { static_cast<Runtime*>(lv_timer_get_user_data(t))->script_step(); }, 20, this);
}

void Runtime::script_step() {
    if (script_pos_ >= script_.size()) return;
    if (static_cast<int32_t>(lv_tick_get() - script_wait_until_) < 0) return;

    auto tap = [this](uint32_t key) {
        on_key(key, KeyPhase::Press);
        on_key(key, KeyPhase::Release);
    };

    const char c = script_[script_pos_];
    if (c != '{' || (script_pos_ + 1 < script_.size() && script_[script_pos_ + 1] == '{')) {
        tap(static_cast<uint8_t>(c));
        script_pos_ += c == '{' ? 2 : 1;
        script_wait_until_ = lv_tick_get() + 30;
        return;
    }
    const size_t close = script_.find('}', script_pos_);
    if (close == std::string::npos) {
        script_pos_ = script_.size();
        return;
    }
    const std::string token = script_.substr(script_pos_ + 1, close - script_pos_ - 1);
    script_pos_ = close + 1;
    script_wait_until_ = lv_tick_get() + 60;

    namespace k = platform::key;
    static const std::pair<const char*, uint32_t> names[] = {
        {"enter", k::Enter}, {"tab", k::Tab}, {"stab", k::ShiftTab}, {"esc", k::Esc},
        {"bs", k::Backspace}, {"del", k::Delete}, {"up", k::Up}, {"down", k::Down},
        {"left", k::Left}, {"right", k::Right}, {"home", k::Home}, {"end", k::End},
        {"help", k::Help},
    };
    for (const auto& [name, code] : names) {
        if (token == name) {
            tap(code);
            return;
        }
    }
    if (token.size() >= 2 && token[0] == 'f' && std::isdigit(static_cast<unsigned char>(token[1]))) {
        tap(k::F(std::atoi(token.c_str() + 1)));
    } else if (token.rfind("wait:", 0) == 0) {
        script_wait_until_ = lv_tick_get() + static_cast<uint32_t>(std::atoi(token.c_str() + 5));
    } else if (token.rfind("shot:", 0) == 0) {
        lv_refr_now(display_);
        const auto r = platform::screenshot::capture_active_screen(token.substr(5));
        LOG_INFO("script screenshot {}: {}", token.substr(5), r.success ? "ok" : r.error);
    } else if (token == "quit") {
        quit();
    } else {
        LOG_WARN("script: unknown token {{{}}}", token);
    }
}

int Runtime::run() {
    running_ = true;
    while (running_
#if USE_DESKTOP
           && impl_->frame->process_events()
#endif
    ) {
        // lv_delay_ms() busy-waits without a delay callback; really sleep to save battery.
        // Input is polled by an LVGL timer, so sleeping until the next timer is safe.
        const uint32_t idle = lv_timer_handler();
        const uint32_t ms = idle == LV_NO_TIMER_READY ? 20 : (idle < 1 ? 1 : (idle > 20 ? 20 : idle));
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
    return 0;
}

}  // namespace cz
