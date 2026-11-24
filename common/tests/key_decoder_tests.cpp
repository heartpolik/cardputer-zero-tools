// Host tests for the evdev key decoder.
#include "key_decoder.h"

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

using platform::KeyDecoder;
namespace key = platform::key;

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool cond, const std::string& what) {
    ++g_checks;
    if (!cond) {
        ++g_failures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

// press + release, returns the key reported on press (0 if none)
uint32_t tap(KeyDecoder& d, uint16_t code) {
    const auto down = d.feed(code, 1);
    const auto up = d.feed(code, 0);
    if (down && up) check(down->key == up->key && !up->pressed, "release matches press");
    return down ? down->key : 0;
}

constexpr uint16_t KEY_ESC = 1, KEY_1 = 2, KEY_0 = 11, KEY_BACKSPACE = 14, KEY_TAB = 15, KEY_Q = 16,
                   KEY_ENTER = 28, KEY_A = 30, KEY_LEFTSHIFT = 42, KEY_Z = 44, KEY_M = 50,
                   KEY_SPACE = 57, KEY_F1 = 59, KEY_UP = 103, KEY_LEFT = 105, KEY_DELETE = 111,
                   KEY_SYSRQ = 99;

void test_letters_and_shift() {
    KeyDecoder d;
    check(tap(d, KEY_Q) == 'q', "q");
    check(tap(d, KEY_A) == 'a', "a");
    check(tap(d, KEY_Z) == 'z', "z");
    check(tap(d, KEY_M) == 'm', "m");
    check(tap(d, KEY_1) == '1', "1");
    check(tap(d, KEY_0) == '0', "0");
    check(tap(d, KEY_SPACE) == ' ', "space");

    d.feed(KEY_LEFTSHIFT, 1);
    check(d.modifiers().shift, "shift held");
    check(tap(d, KEY_A) == 'A', "shift a");
    check(tap(d, KEY_1) == '!', "shift 1");
    check(tap(d, KEY_TAB) == key::ShiftTab, "shift tab");
    d.feed(KEY_LEFTSHIFT, 0);
    check(!d.modifiers().shift, "shift released");
    check(tap(d, KEY_A) == 'a', "a after shift");
}

void test_controls() {
    KeyDecoder d;
    check(tap(d, KEY_ESC) == key::Esc, "esc");
    check(tap(d, KEY_ENTER) == key::Enter, "enter");
    check(tap(d, KEY_BACKSPACE) == key::Backspace, "backspace");
    check(tap(d, KEY_DELETE) == key::Delete, "delete");
    check(tap(d, KEY_TAB) == key::Tab, "tab");
    check(tap(d, KEY_UP) == key::Up, "up");
    check(tap(d, KEY_LEFT) == key::Left, "left");
    check(tap(d, KEY_F1) == key::F(1), "F1");
    check(tap(d, KEY_SYSRQ) == key::PrintScreen, "printscreen");
}

void test_builtin_symbols() {
    // the full vendor keymap (code -> symbol)
    const std::vector<std::pair<uint16_t, char>> map = {
        {26, '!'}, {27, '@'}, {39, '#'}, {40, '$'}, {41, '%'}, {43, '^'}, {51, '&'}, {52, '*'},
        {53, '('}, {94, ')'}, {55, '~'}, {69, '`'}, {70, '_'}, {71, '-'}, {72, '+'}, {73, '='},
        {74, '['}, {75, ']'}, {76, '{'}, {77, '}'}, {79, ';'}, {80, ':'}, {81, '\''}, {82, '"'},
        {83, '<'}, {85, '>'}, {86, '\\'}, {89, '|'}, {90, ','}, {91, '.'}, {92, '/'}, {93, '?'},
    };
    check(map.size() == 32, "32 vendor symbols");
    KeyDecoder builtin(true);
    for (const auto& [code, sym] : map) {
        const uint32_t k = tap(builtin, code);
        check(k == static_cast<uint32_t>(sym),
              "builtin code " + std::to_string(code) + " -> '" + std::string(1, sym) + "' got " + std::to_string(k));
    }
    // symbols are already selected by the driver: shift must not change them
    builtin.feed(KEY_LEFTSHIFT, 1);
    check(tap(builtin, 72) == '+', "builtin symbol ignores shift");
    builtin.feed(KEY_LEFTSHIFT, 0);

    // a USB keyboard must not use the vendor table
    KeyDecoder usb(false);
    check(tap(usb, 26) == '[', "usb code 26 is [");
    check(tap(usb, 52) == '.', "usb code 52 is .");
}

void test_release_keeps_press_translation() {
    KeyDecoder d;
    d.feed(KEY_LEFTSHIFT, 1);
    auto down = d.feed(KEY_A, 1);
    d.feed(KEY_LEFTSHIFT, 0);  // shift released before the letter
    auto up = d.feed(KEY_A, 0);
    check(down && down->key == 'A', "pressed as A");
    check(up && up->key == 'A' && !up->pressed, "released as A");

    check(!d.feed(KEY_A, 2), "kernel repeat ignored");
    check(!d.feed(KEY_Q, 0), "release without press ignored");

    d.feed(KEY_Q, 1);
    d.reset();
    check(!d.feed(KEY_Q, 0), "reset drops held keys");
}

}  // namespace

int main() {
    test_letters_and_shift();
    test_controls();
    test_builtin_symbols();
    test_release_keeps_press_translation();
    std::printf("key decoder: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
