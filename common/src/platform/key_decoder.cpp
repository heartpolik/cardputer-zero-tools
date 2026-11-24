#include "key_decoder.h"

namespace platform {
namespace {

// Linux input-event-codes.h values (kept local: the header is Linux-only and
// the decoder is also compiled on macOS for tests).
enum : uint16_t {
    KEY_ESC = 1, KEY_1 = 2, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0,
    KEY_MINUS = 12, KEY_EQUAL = 13, KEY_BACKSPACE = 14, KEY_TAB = 15,
    KEY_Q = 16, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y, KEY_U, KEY_I, KEY_O, KEY_P,
    KEY_LEFTBRACE = 26, KEY_RIGHTBRACE = 27, KEY_ENTER = 28, KEY_LEFTCTRL = 29,
    KEY_A = 30, KEY_S, KEY_D, KEY_F, KEY_G, KEY_H, KEY_J, KEY_K, KEY_L,
    KEY_SEMICOLON = 39, KEY_APOSTROPHE = 40, KEY_GRAVE = 41, KEY_LEFTSHIFT = 42,
    KEY_BACKSLASH = 43,
    KEY_Z = 44, KEY_X, KEY_C, KEY_V, KEY_B, KEY_N, KEY_M,
    KEY_COMMA = 51, KEY_DOT = 52, KEY_SLASH = 53, KEY_RIGHTSHIFT = 54,
    KEY_LEFTALT = 56, KEY_SPACE = 57,
    KEY_F1 = 59, KEY_F10 = 68, KEY_F11 = 87, KEY_F12 = 88,
    KEY_KPENTER = 96, KEY_RIGHTCTRL = 97, KEY_SYSRQ = 99, KEY_RIGHTALT = 100,
    KEY_HOME = 102, KEY_UP = 103, KEY_PAGEUP = 104, KEY_LEFT = 105, KEY_RIGHT = 106,
    KEY_END = 107, KEY_DOWN = 108, KEY_PAGEDOWN = 109, KEY_INSERT = 110, KEY_DELETE = 111,
    KEY_HELP = 138,
};

}  // namespace

uint32_t KeyDecoder::builtin_symbol(uint16_t code) {
    // /usr/share/keymaps/tca8418_keypad_m5stack_keymap.map (all 32 entries)
    switch (code) {
        case 26: return '!';
        case 27: return '@';
        case 39: return '#';
        case 40: return '$';
        case 41: return '%';
        case 43: return '^';
        case 51: return '&';
        case 52: return '*';
        case 53: return '(';
        case 94: return ')';
        case 55: return '~';
        case 69: return '`';
        case 70: return '_';
        case 71: return '-';
        case 72: return '+';
        case 73: return '=';
        case 74: return '[';
        case 75: return ']';
        case 76: return '{';
        case 77: return '}';
        case 79: return ';';
        case 80: return ':';
        case 81: return '\'';
        case 82: return '"';
        case 83: return '<';
        case 85: return '>';
        case 86: return '\\';
        case 89: return '|';
        case 90: return ',';
        case 91: return '.';
        case 92: return '/';
        case 93: return '?';
        default: return 0;
    }
}

uint32_t KeyDecoder::us_key(uint16_t code, bool shift) {
    static constexpr char kRowQ[] = "qwertyuiop";
    static constexpr char kRowA[] = "asdfghjkl";
    static constexpr char kRowZ[] = "zxcvbnm";
    static constexpr char kDigits[] = "1234567890";
    static constexpr char kDigitsShift[] = "!@#$%^&*()";

    auto letter = [shift](char c) -> uint32_t {
        return static_cast<uint32_t>(shift ? c - 'a' + 'A' : c);
    };

    if (code >= KEY_1 && code <= KEY_0) {
        const int i = code - KEY_1;
        return static_cast<uint32_t>(shift ? kDigitsShift[i] : kDigits[i]);
    }
    if (code >= KEY_Q && code <= KEY_P) return letter(kRowQ[code - KEY_Q]);
    if (code >= KEY_A && code <= KEY_L) return letter(kRowA[code - KEY_A]);
    if (code >= KEY_Z && code <= KEY_M) return letter(kRowZ[code - KEY_Z]);
    if (code >= KEY_F1 && code <= KEY_F10) return key::F(code - KEY_F1 + 1);

    switch (code) {
        case KEY_MINUS: return shift ? '_' : '-';
        case KEY_EQUAL: return shift ? '+' : '=';
        case KEY_LEFTBRACE: return shift ? '{' : '[';
        case KEY_RIGHTBRACE: return shift ? '}' : ']';
        case KEY_SEMICOLON: return shift ? ':' : ';';
        case KEY_APOSTROPHE: return shift ? '"' : '\'';
        case KEY_GRAVE: return shift ? '~' : '`';
        case KEY_BACKSLASH: return shift ? '|' : '\\';
        case KEY_COMMA: return shift ? '<' : ',';
        case KEY_DOT: return shift ? '>' : '.';
        case KEY_SLASH: return shift ? '?' : '/';
        case KEY_SPACE: return ' ';
        case KEY_ESC: return key::Esc;
        case KEY_BACKSPACE: return key::Backspace;
        case KEY_TAB: return shift ? key::ShiftTab : key::Tab;
        case KEY_ENTER:
        case KEY_KPENTER: return key::Enter;
        case KEY_F11: return key::F(11);
        case KEY_F12: return key::F(12);
        case KEY_SYSRQ: return key::PrintScreen;
        case KEY_HELP: return key::Help;
        case KEY_HOME: return key::Home;
        case KEY_END: return key::End;
        case KEY_UP: return key::Up;
        case KEY_DOWN: return key::Down;
        case KEY_LEFT: return key::Left;
        case KEY_RIGHT: return key::Right;
        case KEY_PAGEUP: return key::PageUp;
        case KEY_PAGEDOWN: return key::PageDown;
        case KEY_INSERT: return key::Insert;
        case KEY_DELETE: return key::Delete;
        default: return 0;
    }
}

std::optional<DecodedKey> KeyDecoder::feed(uint16_t code, int32_t value) {
    if (value == 2) return std::nullopt;  // kernel autorepeat: repeat is owned by the app
    const bool down = value != 0;

    switch (code) {
        case KEY_LEFTSHIFT: left_shift_ = down; break;
        case KEY_RIGHTSHIFT: right_shift_ = down; break;
        case KEY_LEFTCTRL: left_ctrl_ = down; break;
        case KEY_RIGHTCTRL: right_ctrl_ = down; break;
        case KEY_LEFTALT: left_alt_ = down; break;
        case KEY_RIGHTALT: right_alt_ = down; break;
        default: {
            if (code >= held_.size()) return std::nullopt;
            if (down) {
                uint32_t k = builtin_ ? builtin_symbol(code) : 0;
                if (k == 0) k = us_key(code, mods_.shift);
                if (k == 0) return std::nullopt;
                held_[code] = k;
                return DecodedKey{k, true};
            }
            const uint32_t k = held_[code];
            if (k == 0) return std::nullopt;  // release without a press we saw
            held_[code] = 0;
            return DecodedKey{k, false};
        }
    }
    mods_.shift = left_shift_ || right_shift_;
    mods_.ctrl = left_ctrl_ || right_ctrl_;
    mods_.alt = left_alt_ || right_alt_;
    return std::nullopt;
}

void KeyDecoder::reset() {
    mods_ = {};
    left_shift_ = right_shift_ = left_ctrl_ = right_ctrl_ = left_alt_ = right_alt_ = false;
    held_.fill(0);
}

}  // namespace platform
