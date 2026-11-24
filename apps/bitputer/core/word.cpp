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

uint64_t reorder(uint64_t v, int width, ByteOrder order) {
    const int n = width / 8;
    uint8_t b[8];
    for (int i = 0; i < n; ++i) b[i] = static_cast<uint8_t>(v >> ((n - 1 - i) * 8));  // b[0] = MSB
    uint8_t out[8];
    for (int i = 0; i < n; ++i) {
        int src = i;
        switch (order) {
            case ByteOrder::AsIs: break;
            case ByteOrder::Reversed: src = n - 1 - i; break;
            case ByteOrder::ByteSwap16: src = n >= 2 ? (i ^ 1) : i; break;
            case ByteOrder::WordSwap16: src = n >= 2 ? (n - 2 - (i & ~1)) + (i & 1) : i; break;
        }
        out[i] = b[src];
    }
    uint64_t r = 0;
    for (int i = 0; i < n; ++i) r = (r << 8) | out[i];
    return r;
}

std::string order_label(int width, ByteOrder order) {
    const int n = width / 8;
    uint64_t letters = 0;  // 'A'.. as bytes, then reordered like a value
    for (int i = 0; i < n; ++i) letters = (letters << 8) | static_cast<uint64_t>('A' + i);
    const uint64_t r = reorder(letters, width, order);
    std::string out;
    for (int i = n - 1; i >= 0; --i) out += static_cast<char>((r >> (i * 8)) & 0xFF);
    return out;
}

std::string byte_string(uint64_t v, int width) {
    static const char* digits = "0123456789ABCDEF";
    std::string out;
    for (int i = width / 8 - 1; i >= 0; --i) {
        const unsigned b = static_cast<unsigned>((v >> (i * 8)) & 0xFF);
        if (!out.empty()) out += ' ';
        out += digits[b >> 4];
        out += digits[b & 15];
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
