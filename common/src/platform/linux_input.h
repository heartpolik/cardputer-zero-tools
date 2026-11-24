// Keyboard input for CardputerZero apps (device evdev + desktop simulator).
//
// All keys are routed to one application listener with explicit phases:
//   Press     - key went down
//   Repeat    - still held (after kRepeatDelayMs, every kRepeatIntervalMs)
//   LongPress - held for kLongPressMs (sent once, in addition to repeats)
//   Release   - key went up
// The listener returns true when it consumed the key; unconsumed presses are
// still delivered to LVGL's focused widget through the keypad indev.
#pragma once

#include "keys.h"
#include "lvgl.h"

#include <cstdint>

namespace platform {

enum class KeyPhase { Press, Repeat, LongPress, Release };

constexpr uint32_t kRepeatDelayMs = 420;
constexpr uint32_t kRepeatIntervalMs = 55;
constexpr uint32_t kLongPressMs = 700;

using KeyListener = bool (*)(uint32_t key, KeyPhase phase, void* user_data);

// Device: opens the built-in keyboard (and any other evdev keyboards).
void init_key_input(lv_display_t* display);
// Routes an LVGL keypad indev (used by the desktop simulator) through the listener.
void attach_key_router(lv_indev_t* indev);

void set_key_listener(KeyListener listener, void* user_data);
void clear_key_listener(KeyListener listener, void* user_data);

// Current Shift/Ctrl/Alt state of the device keyboard (always false on desktop).
Modifiers current_modifiers();

const char* describe_key(uint32_t key);

// Back-compat aliases used by the simulator code.
constexpr uint32_t kSpecialKeyBase = key::kSpecialBase;
constexpr uint32_t kKeyPrintScreen = key::PrintScreen;
constexpr uint32_t kKeyHelp = key::Help;

}  // namespace platform
