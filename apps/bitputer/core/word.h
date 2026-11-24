// Machine words: width, signedness, wrapping and text formats (pure C++).
#pragma once

#include <cstdint>
#include <string>

namespace bits {

enum class Base { Dec = 10, Hex = 16, Oct = 8, Bin = 2 };

const char* base_name(Base b);
Base next_base(Base b);

struct Word {
    int width{32};  // 8, 16, 32 or 64
    bool is_signed{false};

    uint64_t mask() const { return width >= 64 ? ~uint64_t{0} : (uint64_t{1} << width) - 1; }
    uint64_t wrap(uint64_t v) const { return v & mask(); }
    // two's complement value of the low `width` bits
    int64_t to_signed(uint64_t v) const;
    bool negative(uint64_t v) const { return (v >> (width - 1)) & 1; }
    // a value as the word's own interpretation, for comparisons
    bool less(uint64_t a, uint64_t b) const {
        return is_signed ? to_signed(a) < to_signed(b) : wrap(a) < wrap(b);
    }
};

int next_width(int width);

// 0x1234_ABCD (zero-padded to the width, '_' every 4 digits)
std::string format_hex(uint64_t v, int width);
std::string format_dec(uint64_t v, const Word& w, bool as_signed);
std::string format_oct(uint64_t v);  // 0o377
// "0000 1111" (padded to the width, space every 4 digits)
std::string format_bin(uint64_t v, int width);

// Byte orders of a multi-byte value. Labels use A = the most significant byte
// as given: ABCD (big-endian, as is), DCBA (little-endian, all bytes reversed),
// BADC (bytes swapped inside each 16-bit word), CDAB (16-bit words reversed,
// the usual "word swap" of Modbus floats).
enum class ByteOrder { AsIs, Reversed, ByteSwap16, WordSwap16 };
constexpr ByteOrder kByteOrders[] = {ByteOrder::AsIs, ByteOrder::Reversed, ByteOrder::ByteSwap16,
                                     ByteOrder::WordSwap16};

uint64_t reorder(uint64_t v, int width, ByteOrder order);
std::string order_label(int width, ByteOrder order);  // "CDAB"
std::string byte_string(uint64_t v, int width);       // "12 34 56 78"

// A literal that the expression parser reads back in `input` base mode.
std::string literal(uint64_t v, const Word& w, Base input);

}  // namespace bits
