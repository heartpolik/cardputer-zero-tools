#include "serial_format.h"

#include <algorithm>
#include <cctype>

namespace ser {

const std::vector<int>& standard_bauds() {
    static const std::vector<int> bauds = {1200,  2400,   4800,   9600,   19200,  38400,
                                           57600, 115200, 230400, 460800, 921600};
    return bauds;
}

int next_baud(int baud, int dir) {
    const auto& b = standard_bauds();
    const int n = static_cast<int>(b.size());
    int idx = 0;
    while (idx < n && b[static_cast<size_t>(idx)] < baud) ++idx;
    if (idx < n && b[static_cast<size_t>(idx)] == baud) {
        idx = (idx + dir + n) % n;
    } else {
        // not a standard value: step to the neighbour on that side
        idx = dir > 0 ? (idx < n ? idx : 0) : (idx > 0 ? idx - 1 : n - 1);
    }
    return b[static_cast<size_t>(idx)];
}

bool parse_frame(const std::string& text, SerialFormat& fmt) {
    if (text.size() != 3) return false;
    const char d = text[0];
    const char p = static_cast<char>(std::toupper(static_cast<unsigned char>(text[1])));
    const char s = text[2];
    if (d < '5' || d > '8') return false;
    if (p != 'N' && p != 'E' && p != 'O') return false;
    if (s != '1' && s != '2') return false;
    fmt.data_bits = d - '0';
    fmt.parity = p;
    fmt.stop_bits = s - '0';
    return true;
}

std::string frame_name(const SerialFormat& fmt) {
    std::string s;
    s += static_cast<char>('0' + fmt.data_bits);
    s += fmt.parity;
    s += static_cast<char>('0' + fmt.stop_bits);
    return s;
}

const std::vector<std::string>& common_frames() {
    static const std::vector<std::string> frames = {"8N1", "8E1", "8O1", "8N2", "7E1", "7O1"};
    return frames;
}

std::string next_frame(const std::string& name, int dir) {
    const auto& f = common_frames();
    const int n = static_cast<int>(f.size());
    const auto it = std::find(f.begin(), f.end(), name);
    const int idx = it == f.end() ? 0 : static_cast<int>(it - f.begin());
    return f[static_cast<size_t>((idx + dir + n) % n)];
}

int char_bits(const SerialFormat& fmt) { return 1 + fmt.data_bits + (fmt.parity == 'N' ? 0 : 1) + fmt.stop_bits; }

uint32_t char_time_us(const SerialFormat& fmt) {
    const int baud = fmt.baud > 0 ? fmt.baud : 9600;
    return static_cast<uint32_t>((static_cast<uint64_t>(char_bits(fmt)) * 1000000 + baud - 1) / baud);
}

uint32_t modbus_gap_us(const SerialFormat& fmt) {
    if (fmt.baud > 19200) return 1750;
    const int baud = fmt.baud > 0 ? fmt.baud : 9600;
    return static_cast<uint32_t>((static_cast<uint64_t>(char_bits(fmt)) * 3500000 + baud - 1) / baud);
}

}  // namespace ser
