#include "word.h"

namespace bits {

const char* base_name(Base b) {
    switch (b) {
        case Base::Dec: return "DEC";
        case Base::Hex: return "HEX";
        case Base::Oct: return "OCT";
        case Base::Bin: return "BIN";
    }
    return "?";
}

Base next_base(Base b) {
    switch (b) {
        case Base::Dec: return Base::Hex;
        case Base::Hex: return Base::Bin;
        case Base::Bin: return Base::Oct;
        case Base::Oct: return Base::Dec;
    }
    return Base::Dec;
}

int next_width(int width) {
    switch (width) {
        case 8: return 16;
        case 16: return 32;
        case 32: return 64;
        default: return 8;
    }
}

int64_t Word::to_signed(uint64_t v) const {
    v = wrap(v);
    if (width < 64 && negative(v)) v |= ~mask();
    return static_cast<int64_t>(v);
}

std::string format_hex(uint64_t v, int width) {
    static const char* digits = "0123456789ABCDEF";
    const int n = width / 4;
    std::string out = "0x";
    for (int i = n - 1; i >= 0; --i) {
        out += digits[(v >> (i * 4)) & 0xF];
        if (i > 0 && i % 4 == 0) out += '_';
    }
    return out;
}

std::string format_dec(uint64_t v, const Word& w, bool as_signed) {
    if (as_signed) return std::to_string(w.to_signed(v));
    return std::to_string(w.wrap(v));
}

std::string format_oct(uint64_t v) {
    std::string digits;
    do {
        digits.insert(digits.begin(), static_cast<char>('0' + (v & 7)));
        v >>= 3;
    } while (v);
    return "0o" + digits;
}

std::string format_bin(uint64_t v, int width) {
    std::string out;
    for (int i = width - 1; i >= 0; --i) {
        out += ((v >> i) & 1) ? '1' : '0';
        if (i > 0 && i % 4 == 0) out += ' ';
    }
    return out;
}

std::string literal(uint64_t v, const Word& w, Base input) {
    v = w.wrap(v);
    switch (input) {
        case Base::Dec:
            return format_dec(v, w, w.is_signed);
        case Base::Hex:
        case Base::Oct:
        case Base::Bin: {
            // plain digits of the input base (prefixes would be ambiguous in HEX mode: 0b1 = 0xB1)
            const unsigned base = static_cast<unsigned>(input);
            static const char* digits = "0123456789ABCDEF";
            std::string s;
            do {
                s.insert(s.begin(), digits[v % base]);
                v /= base;
            } while (v);
            return s;
        }
    }
    return std::to_string(v);
}

}  // namespace bits
