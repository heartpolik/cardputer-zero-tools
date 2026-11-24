// Translates raw Linux evdev key records into application keys (see keys.h).
// Pure C++, no LVGL or device I/O, so it is covered by host tests.
//
// CardputerZero built-in keyboard (tca8418c) specifics:
//  * the driver already applies the Sym layer and emits symbol codes from the
//    vendor keymap (e.g. code 72 = '+'), which must be looked up before the
//    US layout (on a USB keyboard 72 is KP8);
//  * the driver already applies Fn (arrows, F-keys, Home/End...);
//  * Shift/Ctrl/Alt arrive as ordinary modifier keys.
#pragma once

#include "keys.h"

#include <array>
#include <cstdint>
#include <optional>

namespace platform {

struct DecodedKey {
    uint32_t key{0};
    bool pressed{false};
};

class KeyDecoder {
public:
    explicit KeyDecoder(bool builtin_keyboard = false) : builtin_(builtin_keyboard) {}

    // value: 1 = press, 0 = release, 2 = kernel autorepeat (ignored: the app owns repeat).
    std::optional<DecodedKey> feed(uint16_t code, int32_t value);

    // Drops held keys/modifiers (e.g. after SYN_DROPPED or device loss).
    void reset();

    const Modifiers& modifiers() const { return mods_; }
    bool builtin() const { return builtin_; }

    // Translation tables, exposed for tests.
    static uint32_t builtin_symbol(uint16_t code);
    static uint32_t us_key(uint16_t code, bool shift);

private:
    bool builtin_;
    Modifiers mods_;
    bool left_shift_{false}, right_shift_{false};
    bool left_ctrl_{false}, right_ctrl_{false};
    bool left_alt_{false}, right_alt_{false};
    // press-time translation of each held code, so the release matches the press
    std::array<uint32_t, 256> held_{};
};

}  // namespace platform
