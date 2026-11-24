// Key codes delivered to applications. Printable keys are their Unicode code
// point; controls reuse LVGL's LV_KEY_* values (checked by static_assert in
// linux_input.cpp) so this header stays free of LVGL for host tests.
#pragma once

#include <cstdint>

namespace platform {

namespace key {
constexpr uint32_t Home = 2;       // LV_KEY_HOME
constexpr uint32_t End = 3;        // LV_KEY_END
constexpr uint32_t Backspace = 8;  // LV_KEY_BACKSPACE
constexpr uint32_t Tab = 9;        // LV_KEY_NEXT
constexpr uint32_t Enter = 10;     // LV_KEY_ENTER
constexpr uint32_t ShiftTab = 11;  // LV_KEY_PREV
constexpr uint32_t Up = 17;        // LV_KEY_UP
constexpr uint32_t Down = 18;      // LV_KEY_DOWN
constexpr uint32_t Right = 19;     // LV_KEY_RIGHT
constexpr uint32_t Left = 20;      // LV_KEY_LEFT
constexpr uint32_t Esc = 27;       // LV_KEY_ESC
constexpr uint32_t Delete = 127;   // LV_KEY_DEL

// Non-text keys outside the Unicode range.
constexpr uint32_t kSpecialBase = 0x200000U;
constexpr uint32_t F1 = kSpecialBase | 1U;  // F1..F12 are consecutive
constexpr uint32_t F12 = kSpecialBase | 12U;
constexpr uint32_t PrintScreen = kSpecialBase | 18U;
constexpr uint32_t Help = kSpecialBase | 19U;
constexpr uint32_t PageUp = kSpecialBase | 20U;
constexpr uint32_t PageDown = kSpecialBase | 21U;
constexpr uint32_t Insert = kSpecialBase | 22U;

constexpr uint32_t F(int n) { return kSpecialBase | static_cast<uint32_t>(n); }
constexpr bool is_printable(uint32_t k) { return k >= 32 && k < 127; }
}  // namespace key

struct Modifiers {
    bool shift{false};
    bool ctrl{false};
    bool alt{false};
};

}  // namespace platform
