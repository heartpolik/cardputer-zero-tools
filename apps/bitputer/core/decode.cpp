#include "decode.h"

#include <cstdio>
#include <cstring>

namespace bits {
namespace {

// days since 1970-01-01 -> civil date (Howard Hinnant's algorithm)
void civil_from_days(int64_t z, int64_t& y, unsigned& m, unsigned& d) {
    z += 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    y = static_cast<int64_t>(yoe) + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    d = doy - (153 * mp + 2) / 5 + 1;
    m = mp < 10 ? mp + 3 : mp - 9;
    if (m <= 2) ++y;
}

}  // namespace

const char* decode_name(Decode d) {
    switch (d) {
        case Decode::Chars: return "CHR";
        case Decode::Float: return "FLT";
        case Decode::Time: return "UTC";
    }
    return "?";
}

Decode next_decode(Decode d) {
    switch (d) {
        case Decode::Chars: return Decode::Float;
        case Decode::Float: return Decode::Time;
        case Decode::Time: return Decode::Chars;
    }
    return Decode::Chars;
}

std::string decode_chars(uint64_t v, int width) {
    std::string out = "'";
    for (int i = width / 8 - 1; i >= 0; --i) {
        const unsigned char c = static_cast<unsigned char>((v >> (i * 8)) & 0xFF);
        out += (c >= 32 && c < 127) ? static_cast<char>(c) : '.';
    }
    return out + "'";
}

std::string decode_float(uint64_t v, int width) {
    char buf[64];
    if (width == 32) {
        const uint32_t b = static_cast<uint32_t>(v);
        float f;
        std::memcpy(&f, &b, sizeof f);
        std::snprintf(buf, sizeof buf, "f32 %.9g", static_cast<double>(f));
    } else if (width == 64) {
        double d;
        std::memcpy(&d, &v, sizeof d);
        std::snprintf(buf, sizeof buf, "f64 %.17g", d);
    } else {
        return "float needs a 32 or 64-bit word";
    }
    return buf;
}

std::string decode_time(uint64_t v, const Word& w) {
    int64_t t = w.is_signed ? w.to_signed(v) : static_cast<int64_t>(w.wrap(v));
    if (!w.is_signed && w.wrap(v) > static_cast<uint64_t>(INT64_MAX)) return "out of range";
    const char* unit = "s";
    int ms = -1;
    if (t > 100000000000LL || t < -100000000000LL) {  // > year 5138 as seconds: milliseconds
        ms = static_cast<int>(((t % 1000) + 1000) % 1000);
        t = (t - ms) / 1000;
        unit = "ms";
    }
    int64_t days = t / 86400;
    int64_t secs = t % 86400;
    if (secs < 0) {
        secs += 86400;
        --days;
    }
    int64_t y;
    unsigned mo, d;
    civil_from_days(days, y, mo, d);
    if (y < -9999 || y > 99999) return "out of range";
    char buf[64];
    const int hh = static_cast<int>(secs / 3600), mm = static_cast<int>(secs / 60 % 60), ss = static_cast<int>(secs % 60);
    if (ms >= 0) {
        std::snprintf(buf, sizeof buf, "%04lld-%02u-%02u %02d:%02d:%02d.%03d (%s)", static_cast<long long>(y), mo, d,
                      hh, mm, ss, ms, unit);
    } else {
        std::snprintf(buf, sizeof buf, "%04lld-%02u-%02u %02d:%02d:%02d", static_cast<long long>(y), mo, d, hh, mm,
                      ss);
    }
    return buf;
}

std::string decode(Decode d, uint64_t v, const Word& w) {
    switch (d) {
        case Decode::Chars: return decode_chars(w.wrap(v), w.width);
        case Decode::Float: return decode_float(w.wrap(v), w.width);
        case Decode::Time: return decode_time(v, w);
    }
    return "";
}

}  // namespace bits
