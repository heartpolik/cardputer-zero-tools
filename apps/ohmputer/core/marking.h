// Component markings: resistor colour bands and printed codes (pure C++).
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ohm {

// ---- colour bands (IEC 60062) ----

enum class Color { Black, Brown, Red, Orange, Yellow, Green, Blue, Violet, Grey, White, Gold, Silver, None };

const char* color_name(Color c);   // "violet"
const char* color_short(Color c);  // "VIO"
uint32_t color_rgb(Color c);
// keys: 0..9 = black..white (the digit the colour stands for), g = gold, s = silver
std::optional<Color> color_from_key(char k);

struct BandReading {
    bool ok{false};
    double ohms{0};
    double tolerance{0};  // 0.05 = ±5 %
    int tempco{0};        // ppm/K, 6-band only
    std::string error;
    bool reversed{false};  // read from the other end
};

// 3 bands (2 digits + multiplier, ±20 %), 4, 5 or 6 bands. Tries the other
// reading direction when the given one is impossible (e.g. starts with gold).
BandReading decode_bands(const std::vector<Color>& bands);

// value -> 4-band (2 significant digits) / 5-band (3 digits) codes; empty when
// the value needs more digits than the code has.
std::vector<Color> encode_bands(double ohms, int digits, double tolerance);

// ---- printed codes ----

struct Reading {
    char kind;          // 'R' resistor, 'C' capacitor, 'L' inductor, 'D' diode, 'Q' transistor, 'U' other
    std::string how;    // "3-digit SMD", "EIA-96", "RKM", "part code"...
    std::string value;  // "4.7 kΩ ±1 %", "100 nF 100 V ±5 %", "1N4007 1 A 1000 V rectifier"
    double number{-1};  // resistors: ohms (for drawing the colour bands), else -1
};

// Every plausible reading of a marking: "103", "4R7", "1002", "01C", "104K",
// "2A104J", "4n7", "A106", "M7", "1AM" ...
std::vector<Reading> decode_marking(const std::string& code);

}  // namespace ohm
