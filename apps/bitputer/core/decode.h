// Other readings of the same bits: chars, IEEE-754 floats, Unix time (pure C++).
#pragma once

#include "word.h"

#include <cstdint>
#include <string>

namespace bits {

enum class Decode { Chars, Float, Time };

const char* decode_name(Decode d);
Decode next_decode(Decode d);

// 'AB.D' big-endian bytes of the word, non-printables as '.'
std::string decode_chars(uint64_t v, int width);
// width 32 -> float, 64 -> double, smaller: the low bits are too short
std::string decode_float(uint64_t v, int width);
// seconds since 1970 (milliseconds when > 1e11) -> "2026-11-24 10:00:00 UTC"
std::string decode_time(uint64_t v, const Word& w);

std::string decode(Decode d, uint64_t v, const Word& w);

}  // namespace bits
