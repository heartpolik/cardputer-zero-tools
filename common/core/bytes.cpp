#include "bytes.h"

#include <cctype>
#include <cstdio>

namespace cz {
namespace {

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

uint32_t reflect(uint32_t v, int bits) {
    uint32_t r = 0;
    for (int i = 0; i < bits; ++i) {
        r = (r << 1) | (v & 1);
        v >>= 1;
    }
    return r;
}

ByteParse parse_text(const std::string& s, size_t open) {
    ByteParse r;
    r.text = true;
    size_t i = open + 1;
    while (i < s.size() && s[i] != '"') {
        char c = s[i++];
        if (c == '\\' && i < s.size()) {
            const char e = s[i++];
            if (e == 'x') {
                const int h = i < s.size() ? hex_value(s[i]) : -1;
                const int l = i + 1 < s.size() ? hex_value(s[i + 1]) : -1;
                if (h < 0 || l < 0) {
                    r.error = "\\x needs two hex digits";
                    r.error_pos = i - 2;
                    return r;
                }
                r.bytes.push_back(static_cast<uint8_t>(h * 16 + l));
                i += 2;
                continue;
            }
            c = e == 'n' ? '\n' : e == 'r' ? '\r' : e == 't' ? '\t' : e == '0' ? '\0' : e;
        }
        r.bytes.push_back(static_cast<uint8_t>(c));
    }
    // a missing closing quote is fine while typing; text after it is not
    if (i < s.size()) {
        ++i;
        while (i < s.size() && s[i] == ' ') ++i;
        if (i < s.size()) {
            r.error = "text after the closing quote";
            r.error_pos = i;
            return r;
        }
    }
    r.ok = true;
    return r;
}

}  // namespace

ByteParse parse_bytes(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && s[i] == ' ') ++i;
    if (i < s.size() && s[i] == '"') return parse_text(s, i);

    ByteParse r;
    while (i < s.size()) {
        const char c = s[i];
        if (c == ' ' || c == ',' || c == ':' || c == '-' || c == ';') {
            ++i;
            continue;
        }
        // one token of hex digits, optional 0x prefix
        size_t start = i;
        if (c == '0' && i + 1 < s.size() && (s[i + 1] == 'x' || s[i + 1] == 'X')) start = i += 2;
        size_t end = start;
        while (end < s.size() && hex_value(s[end]) >= 0) ++end;
        if (end == start) {
            r.error = std::string("'") + s[i] + "' is not a hex digit";
            r.error_pos = i;
            return r;
        }
        const size_t n = end - start;
        if (n % 2 == 1 && n > 1) {
            r.error = "odd number of hex digits";
            r.error_pos = start;
            return r;
        }
        if (n == 1) {
            r.bytes.push_back(static_cast<uint8_t>(hex_value(s[start])));
        } else {
            for (size_t k = start; k < end; k += 2) {
                r.bytes.push_back(static_cast<uint8_t>(hex_value(s[k]) * 16 + hex_value(s[k + 1])));
            }
        }
        i = end;
    }
    r.ok = true;
    return r;
}

const std::vector<CrcSpec>& checksums() {
    static const std::vector<CrcSpec> specs = {
        {"CRC-16/MODBUS", "Modbus RTU, inverters, BMS", 16, 0x8005, 0xFFFF, true, true, 0x0000, true},
        {"CRC-32", "zip, Ethernet, PNG", 32, 0x04C11DB7, 0xFFFFFFFF, true, true, 0xFFFFFFFF, false},
        {"CRC-32C", "iSCSI, ext4, SCTP", 32, 0x1EDC6F41, 0xFFFFFFFF, true, true, 0xFFFFFFFF, false},
        {"CRC-16/CCITT", "CCITT-FALSE, many radios", 16, 0x1021, 0xFFFF, false, false, 0x0000, false},
        {"CRC-16/XMODEM", "XMODEM, ZigBee", 16, 0x1021, 0x0000, false, false, 0x0000, false},
        {"CRC-16/KERMIT", "Kermit, Bluetooth", 16, 0x1021, 0x0000, true, true, 0x0000, true},
        {"CRC-16/ARC", "CRC-16/IBM, USB", 16, 0x8005, 0x0000, true, true, 0x0000, true},
        {"CRC-8", "SMBus PEC", 8, 0x07, 0x00, false, false, 0x00, false},
        {"CRC-8/MAXIM", "1-Wire, DS18B20", 8, 0x31, 0x00, true, true, 0x00, false},
        {"CRC-8/NRSC-5", "Sensirion SHT3x/SCD", 8, 0x31, 0xFF, false, false, 0x00, false},
        {"XOR8", "NMEA *hh, simple frames", 0, 2, 0, false, false, 0, false},
        {"SUM8", "byte sum mod 256", 0, 1, 0, false, false, 0, false},
    };
    return specs;
}

uint32_t compute(const CrcSpec& spec, const std::vector<uint8_t>& data) {
    if (spec.width == 0) {
        uint32_t acc = 0;
        for (uint8_t b : data) acc = spec.poly == 1 ? (acc + b) & 0xFF : acc ^ b;
        return acc;
    }
    const int w = spec.width;
    const uint32_t top = uint32_t{1} << (w - 1);
    const uint32_t mask = w == 32 ? 0xFFFFFFFFu : (uint32_t{1} << w) - 1;
    uint32_t crc = spec.init & mask;
    for (uint8_t byte : data) {
        const uint32_t in = spec.refin ? reflect(byte, 8) : byte;
        crc ^= in << (w - 8);
        for (int i = 0; i < 8; ++i) {
            crc = (crc & top) ? ((crc << 1) ^ spec.poly) : (crc << 1);
            crc &= mask;
        }
    }
    if (spec.refout) crc = reflect(crc, w);
    return (crc ^ spec.xorout) & mask;
}

std::string wire_bytes(const CrcSpec& spec, uint32_t value) {
    const int n = spec.width == 0 ? 1 : spec.width / 8;
    std::string out;
    char buf[4];
    for (int i = 0; i < n; ++i) {
        const int shift = spec.little_endian_on_wire ? i * 8 : (n - 1 - i) * 8;
        std::snprintf(buf, sizeof buf, "%02X", static_cast<unsigned>((value >> shift) & 0xFF));
        if (!out.empty()) out += ' ';
        out += buf;
    }
    return out;
}

}  // namespace cz
