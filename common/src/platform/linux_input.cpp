#include "linux_input.h"

#include "key_decoder.h"

#include <cstdio>
#include <cstring>

#if !USE_DESKTOP
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#ifndef APP_KEY_INPUT_DEVICE
#define APP_KEY_INPUT_DEVICE ""
#endif

namespace platform {

static_assert(key::Home == LV_KEY_HOME);
static_assert(key::End == LV_KEY_END);
static_assert(key::Backspace == LV_KEY_BACKSPACE);
static_assert(key::Tab == LV_KEY_NEXT);
static_assert(key::Enter == LV_KEY_ENTER);
static_assert(key::ShiftTab == LV_KEY_PREV);
static_assert(key::Up == LV_KEY_UP);
static_assert(key::Down == LV_KEY_DOWN);
static_assert(key::Right == LV_KEY_RIGHT);
static_assert(key::Left == LV_KEY_LEFT);
static_assert(key::Esc == LV_KEY_ESC);
static_assert(key::Delete == LV_KEY_DEL);

namespace {

KeyListener g_listener = nullptr;
void* g_listener_data = nullptr;

// one held key at a time drives repeat / long press
uint32_t g_held_key = 0;
uint32_t g_held_since = 0;
uint32_t g_last_repeat = 0;
bool g_long_sent = false;
lv_timer_t* g_hold_timer = nullptr;
Modifiers g_modifiers;

bool emit(uint32_t key, KeyPhase phase) {
    return g_listener && g_listener(key, phase, g_listener_data);
}

void hold_timer_cb(lv_timer_t*) {
    if (!g_held_key) return;
    const uint32_t key = g_held_key;
    const uint32_t held_for = lv_tick_elaps(g_held_since);
    if (!g_long_sent && held_for >= kLongPressMs) {
        g_long_sent = true;
        emit(key, KeyPhase::LongPress);
        if (g_held_key != key) return;  // listener may have reset state
    }
    if (held_for >= kRepeatDelayMs && lv_tick_elaps(g_last_repeat) >= kRepeatIntervalMs) {
        g_last_repeat = lv_tick_get();
        emit(key, KeyPhase::Repeat);
    }
}

void start_hold(uint32_t key) {
    g_held_key = key;
    g_held_since = lv_tick_get();
    g_last_repeat = g_held_since;
    g_long_sent = false;
    if (!g_hold_timer) g_hold_timer = lv_timer_create(hold_timer_cb, 15, nullptr);
    lv_timer_resume(g_hold_timer);
}

void stop_hold() {
    g_held_key = 0;
    if (g_hold_timer) lv_timer_pause(g_hold_timer);
}

// Returns true when the application consumed the event.
bool route(uint32_t key, bool pressed) {
    if (pressed) {
        start_hold(key);
        return emit(key, KeyPhase::Press);
    }
    if (key == g_held_key) stop_hold();
    return emit(key, KeyPhase::Release);
}

// Desktop simulator keypad: LV_EVENT_KEY from its indev.
void key_event_cb(lv_event_t*) {
    auto* indev = lv_indev_active();
    if (!indev) return;
    const auto key = lv_indev_get_key(indev);
    const bool pressed = lv_indev_get_state(indev) == LV_INDEV_STATE_PRESSED;
    static uint32_t last_key = 0;
    static bool last_pressed = false;
    // LVGL re-sends LV_EVENT_KEY while a key stays pressed; only route edges
    if (key == last_key && pressed == last_pressed) return;
    last_key = key;
    last_pressed = pressed;
    route(key, pressed);
}

#if !USE_DESKTOP
struct EvdevKeyboard {
    int fd{-1};
    KeyDecoder decoder;
    bool dropping{false};  // after SYN_DROPPED until SYN_REPORT
    uint32_t lv_key{0};
    lv_indev_state_t lv_state{LV_INDEV_STATE_RELEASED};
};

void evdev_read_cb(lv_indev_t* indev, lv_indev_data_t* data) {
    auto* kb = static_cast<EvdevKeyboard*>(lv_indev_get_driver_data(indev));
    data->continue_reading = false;
    if (!kb || kb->fd < 0) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    // bounded batch so a flood cannot starve the UI thread
    for (int budget = 0; budget < 128; ++budget) {
        input_event ev{};
        const ssize_t n = read(kb->fd, &ev, sizeof(ev));
        if (n != static_cast<ssize_t>(sizeof(ev))) {
            if (n == 0 || (n < 0 && errno != EAGAIN && errno != EINTR)) {
                // device gone: release everything and go inert
                close(kb->fd);
                kb->fd = -1;
                kb->decoder.reset();
                stop_hold();
            }
            break;
        }
        if (ev.type == EV_SYN) {
            if (ev.code == SYN_DROPPED) {
                kb->dropping = true;
                kb->decoder.reset();
                stop_hold();
            } else if (ev.code == SYN_REPORT) {
                kb->dropping = false;
            }
            continue;
        }
        if (kb->dropping || ev.type != EV_KEY) continue;

        const auto decoded = kb->decoder.feed(ev.code, ev.value);
        g_modifiers = kb->decoder.modifiers();
        if (!decoded) continue;

        const bool consumed = route(decoded->key, decoded->pressed);
        if (!consumed || !decoded->pressed) {
            // hand the edge to LVGL (focused widget) and continue next read
            kb->lv_key = decoded->key;
            kb->lv_state = decoded->pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
            data->continue_reading = true;
            break;
        }
    }
    data->key = kb->lv_key;
    data->state = kb->lv_state;
}

void evdev_delete_cb(lv_event_t* event) {
    auto* indev = static_cast<lv_indev_t*>(lv_event_get_target(event));
    auto* kb = static_cast<EvdevKeyboard*>(lv_indev_get_driver_data(indev));
    if (!kb) return;
    if (kb->fd >= 0) close(kb->fd);
    delete kb;
}

bool is_keyboard(int fd) {
    unsigned long bits[(KEY_MAX / (sizeof(unsigned long) * 8)) + 1] = {};
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(bits)), bits) < 0) return false;
    auto has = [&](int code) {
        const int w = static_cast<int>(sizeof(unsigned long) * 8);
        return (bits[code / w] & (1UL << (code % w))) != 0;
    };
    return has(KEY_A) && has(KEY_ENTER);
}

void try_open(const char* path, lv_display_t* display) {
    const int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        LV_LOG_WARN("input: cannot open %s: %s", path, strerror(errno));
        return;
    }
    if (!is_keyboard(fd)) {
        close(fd);
        return;
    }
    char name[128] = {};
    ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name);
    const bool builtin = std::strstr(name, "tca8418") != nullptr;

    auto* kb = new EvdevKeyboard;
    kb->fd = fd;
    kb->decoder = KeyDecoder(builtin);

    auto* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(indev, evdev_read_cb);
    lv_indev_set_driver_data(indev, kb);
    lv_indev_add_event_cb(indev, evdev_delete_cb, LV_EVENT_DELETE, nullptr);
    lv_indev_set_display(indev, display);
    LV_LOG_USER("input: %s (%s)%s", path, name, builtin ? " [built-in]" : "");
}
#endif

}  // namespace

void init_key_input(lv_display_t* display) {
#if !USE_DESKTOP
    const char* configured = APP_KEY_INPUT_DEVICE;
    if (configured[0] != '\0') {
        try_open(configured, display);
        return;
    }
    DIR* dir = opendir("/dev/input");
    if (!dir) return;
    while (auto* entry = readdir(dir)) {
        if (std::strncmp(entry->d_name, "event", 5) != 0) continue;
        const std::string path = std::string("/dev/input/") + entry->d_name;
        try_open(path.c_str(), display);
    }
    closedir(dir);
#else
    LV_UNUSED(display);
#endif
}

void attach_key_router(lv_indev_t* indev) {
    if (!indev || lv_indev_get_type(indev) != LV_INDEV_TYPE_KEYPAD) return;
    lv_indev_add_event_cb(indev, key_event_cb, LV_EVENT_KEY, nullptr);
}

void set_key_listener(KeyListener listener, void* user_data) {
    g_listener = listener;
    g_listener_data = user_data;
}

void clear_key_listener(KeyListener listener, void* user_data) {
    if (g_listener == listener && g_listener_data == user_data) {
        g_listener = nullptr;
        g_listener_data = nullptr;
        stop_hold();
    }
}

Modifiers current_modifiers() { return g_modifiers; }

const char* describe_key(uint32_t k) {
    switch (k) {
        case key::Esc: return "Esc";
        case key::Enter: return "Enter";
        case key::Backspace: return "Backspace";
        case key::Delete: return "Del";
        case key::Tab: return "Tab";
        case key::ShiftTab: return "Shift+Tab";
        case key::Up: return "Up";
        case key::Down: return "Down";
        case key::Left: return "Left";
        case key::Right: return "Right";
        case key::Home: return "Home";
        case key::End: return "End";
        case key::PageUp: return "PageUp";
        case key::PageDown: return "PageDown";
        case key::Insert: return "Insert";
        case key::PrintScreen: return "PrintScreen";
        case key::Help: return "Help";
        default: break;
    }
    static char buf[24];
    if (k >= key::F(1) && k <= key::F(12)) {
        std::snprintf(buf, sizeof(buf), "F%u", k - key::kSpecialBase);
    } else if (key::is_printable(k)) {
        std::snprintf(buf, sizeof(buf), "%c", static_cast<char>(k));
    } else {
        std::snprintf(buf, sizeof(buf), "Key_%u", k);
    }
    return buf;
}

}  // namespace platform
