// Serial line settings and their timing (pure C++).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ser {

struct SerialFormat {
    int baud{9600};
    int data_bits{8};   // 5..8
    char parity{'N'};   // N, E, O
    int stop_bits{1};   // 1 or 2

    bool operator==(const SerialFormat& o) const {
        return baud == o.baud && data_bits == o.data_bits && parity == o.parity && stop_bits == o.stop_bits;
    }
    bool operator!=(const SerialFormat& o) const { return !(*this == o); }
};

const std::vector<int>& standard_bauds();
int next_baud(int baud, int dir);  // neighbour in standard_bauds()

// "8N1", "7E1", "8N2" (case-insensitive); false on anything else
bool parse_frame(const std::string& text, SerialFormat& fmt);
std::string frame_name(const SerialFormat& fmt);  // "8N1"
const std::vector<std::string>& common_frames();  // 8N1 8E1 8O1 8N2 7E1 7O1
std::string next_frame(const std::string& name, int dir);

// bits on the wire per character: start + data + parity + stop
int char_bits(const SerialFormat& fmt);
// microseconds per character
uint32_t char_time_us(const SerialFormat& fmt);
// Modbus RTU inter-frame silence: 3.5 characters, fixed 1750 us above 19200 baud
uint32_t modbus_gap_us(const SerialFormat& fmt);

}  // namespace ser
