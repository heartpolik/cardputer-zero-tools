#include "marking.h"

#include "eseries.h"
#include "si.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace ohm {
namespace {

std::string upper(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

bool all_digits(const std::string& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)); });
}

std::string percent(double tol) {
    if (tol <= 0) return "";
    return "\xC2\xB1" + format_plain(tol * 100, 3) + " %";
}

std::string ohms_text(double v) { return format_si(v, "\xCE\xA9", 4); }

std::string farads_text(double f) {
    std::string s = format_si(f, "F", 3);
    // also the unit people order by: 100 nF = 0.1 µF, 4.7 nF = 4700 pF
    if (f >= 1e-9 && f < 1e-6) s += " = " + format_plain(f * 1e6, 3) + " \xC2\xB5" "F";
    else if (f >= 1e-12 && f < 1e-9 && f * 1e12 >= 10) s += " = " + format_plain(f * 1e9, 3) + " nF";
    return s;
}

std::string henries_text(double h) { return format_si(h, "H", 3); }

std::optional<double> tolerance_letter(char c) {
    switch (std::toupper(static_cast<unsigned char>(c))) {
        case 'B': return 0.001;
        case 'C': return 0.0025;
        case 'D': return 0.005;
        case 'F': return 0.01;
        case 'G': return 0.02;
        case 'J': return 0.05;
        case 'K': return 0.10;
        case 'M': return 0.20;
        default: return std::nullopt;
    }
}

// EIA voltage codes on ceramic / film capacitors ("2A" = 100 V)
std::optional<double> voltage_code(const std::string& code) {
    static const std::pair<const char*, double> codes[] = {
        {"0G", 4},   {"0J", 6.3}, {"1A", 10},  {"1C", 16},  {"1D", 20},  {"1E", 25},  {"1V", 35},
        {"1H", 50},  {"1J", 63},  {"1K", 80},  {"2A", 100}, {"2C", 160}, {"2D", 200}, {"2E", 250},
        {"2V", 350}, {"2G", 400}, {"2W", 450}, {"2H", 500}, {"2J", 630}, {"3A", 1000},
    };
    for (const auto& [c, v] : codes) {
        if (code == c) return v;
    }
    return std::nullopt;
}

// tantalum capacitor voltage letters ("A106" = 10 µF 10 V)
std::optional<double> tantalum_voltage(char c) {
    switch (c) {
        case 'G': return 4;
        case 'J': return 6.3;
        case 'A': return 10;
        case 'C': return 16;
        case 'D': return 20;
        case 'E': return 25;
        case 'V': return 35;
        case 'H': return 50;
        default: return std::nullopt;
    }
}

// 3-digit capacitor code in pF: 8 and 9 multiply by 0.01 and 0.1
std::optional<double> cap_code_pf(const std::string& d) {
    const int m = d[2] - '0';
    const double base = (d[0] - '0') * 10 + (d[1] - '0');
    if (m <= 6) return base * std::pow(10.0, m);
    if (m == 8) return base * 0.01;
    if (m == 9) return base * 0.1;
    return std::nullopt;
}

struct Part {
    const char* code;
    char kind;
    const char* text;
};

// a small list of very common SMD codes; the same code can mean other parts
const Part kParts[] = {
    {"1AM", 'Q', "MMBT3904  NPN 40 V 200 mA"},
    {"2A", 'Q', "MMBT3906  PNP 40 V 200 mA"},
    {"1P", 'Q', "MMBT2222A  NPN 40 V 600 mA"},
    {"2F", 'Q', "MMBT2907A  PNP 60 V 600 mA"},
    {"J3Y", 'Q', "S8050  NPN 25 V 500 mA"},
    {"2TY", 'Q', "S8550  PNP 25 V 500 mA"},
    {"702", 'Q', "2N7002  N-MOSFET 60 V 115 mA"},
    {"A7", 'D', "BAV99  2 diodes in series"},
    {"A4", 'D', "BAV70  2 diodes, common cathode"},
    {"A1", 'D', "BAW56  2 diodes, common anode"},
    {"A6", 'D', "BAS16  switching diode 100 V"},
    {"T4", 'D', "1N4148W  switching diode 100 V"},
    {"M1", 'D', "1N4001 (SMD)  1 A 50 V"},
    {"M2", 'D', "1N4002 (SMD)  1 A 100 V"},
    {"M3", 'D', "1N4003 (SMD)  1 A 200 V"},
    {"M4", 'D', "1N4004 (SMD)  1 A 400 V"},
    {"M5", 'D', "1N4005 (SMD)  1 A 600 V"},
    {"M6", 'D', "1N4006 (SMD)  1 A 800 V"},
    {"M7", 'D', "1N4007 (SMD)  1 A 1000 V"},
    {"SS14", 'D', "Schottky 1 A 40 V"},
    {"SS34", 'D', "Schottky 3 A 40 V"},
    {"SS54", 'D', "Schottky 5 A 40 V"},
};

// RKM code (IEC 60062): one letter is the decimal point and the multiplier
bool decode_rkm(const std::string& s, std::vector<Reading>& out) {
    size_t letter = std::string::npos;
    for (size_t i = 0; i < s.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) {
            if (letter != std::string::npos) return false;
            letter = i;
        }
    }
    if (letter == std::string::npos || s.size() < 2) return false;
    const std::string a = s.substr(0, letter), b = s.substr(letter + 1);
    if (a.empty() && b.empty()) return false;
    const double mant = std::stod((a.empty() ? "0" : a) + "." + (b.empty() ? "0" : b));
    const char L = s[letter];
    const char U = static_cast<char>(std::toupper(static_cast<unsigned char>(L)));
    if (U == 'R') {
        out.push_back({'R', "RKM code", ohms_text(mant), mant});
        out.push_back({'L', "inductor code (\xC2\xB5H)", henries_text(mant * 1e-6)});
        out.push_back({'C', "small capacitor (pF)", farads_text(mant * 1e-12)});
        return true;
    }
    if (U == 'K' || L == 'M' || U == 'G') {
        const double mult = U == 'K' ? 1e3 : U == 'M' ? 1e6 : 1e9;
        out.push_back({'R', "RKM code", ohms_text(mant * mult), mant * mult});
        return true;
    }
    if (U == 'P' || U == 'N' || U == 'U') {
        const double mult = U == 'P' ? 1e-12 : U == 'N' ? 1e-9 : 1e-6;
        out.push_back({'C', "RKM code", farads_text(mant * mult)});
        if (U != 'P') out.push_back({'L', "RKM code", henries_text(mant * mult)});
        return true;
    }
    return false;
}

}  // namespace

// ---------------------------------------------------------------- colours

const char* color_name(Color c) {
    static const char* names[] = {"black", "brown", "red",  "orange", "yellow", "green", "blue",
                                  "violet", "grey", "white", "gold",  "silver", "none"};
    return names[static_cast<int>(c)];
}

const char* color_short(Color c) {
    static const char* names[] = {"BLK", "BRN", "RED", "ORG", "YEL", "GRN", "BLU",
                                  "VIO", "GRY", "WHT", "GLD", "SLV", "---"};
    return names[static_cast<int>(c)];
}

uint32_t color_rgb(Color c) {
    static const uint32_t rgb[] = {0x1a1a1a, 0x8b4513, 0xe53935, 0xfb8c00, 0xfdd835, 0x43a047, 0x1e88e5,
                                   0x8e24aa, 0x9e9e9e, 0xf5f5f5, 0xd4af37, 0xc0c0c0, 0x000000};
    return rgb[static_cast<int>(c)];
}

std::optional<Color> color_from_key(char k) {
    if (k >= '0' && k <= '9') return static_cast<Color>(k - '0');
    if (k == 'g' || k == 'G') return Color::Gold;
    if (k == 's' || k == 'S') return Color::Silver;
    return std::nullopt;
}

namespace {

bool is_digit_color(Color c) { return c <= Color::White; }

std::optional<double> multiplier(Color c) {
    if (is_digit_color(c)) return std::pow(10.0, static_cast<int>(c));
    if (c == Color::Gold) return 0.1;
    if (c == Color::Silver) return 0.01;
    return std::nullopt;
}

std::optional<double> band_tolerance(Color c) {
    switch (c) {
        case Color::Brown: return 0.01;
        case Color::Red: return 0.02;
        case Color::Orange: return 0.0005;
        case Color::Yellow: return 0.0002;
        case Color::Green: return 0.005;
        case Color::Blue: return 0.0025;
        case Color::Violet: return 0.001;
        case Color::Grey: return 0.0001;
        case Color::Gold: return 0.05;
        case Color::Silver: return 0.10;
        default: return std::nullopt;
    }
}

std::optional<int> band_tempco(Color c) {
    switch (c) {
        case Color::Black: return 250;
        case Color::Brown: return 100;
        case Color::Red: return 50;
        case Color::Orange: return 15;
        case Color::Yellow: return 25;
        case Color::Green: return 20;
        case Color::Blue: return 10;
        case Color::Violet: return 5;
        case Color::Grey: return 1;
        default: return std::nullopt;
    }
}

BandReading decode_one_way(const std::vector<Color>& b) {
    BandReading r;
    const size_t n = b.size();
    if (n == 1) {
        if (b[0] == Color::Black) {
            r.ok = true;  // zero-ohm link
            return r;
        }
        r.error = "1 band: only black (0 \xCE\xA9 link)";
        return r;
    }
    if (n < 3 || n > 6) {
        r.error = "3 to 6 bands";
        return r;
    }
    const size_t digits = n <= 4 ? 2 : 3;
    double value = 0;
    for (size_t i = 0; i < digits; ++i) {
        if (!is_digit_color(b[i])) {
            r.error = std::string("band ") + std::to_string(i + 1) + " must be a digit colour";
            return r;
        }
        value = value * 10 + static_cast<int>(b[i]);
    }
    const auto mult = multiplier(b[digits]);
    if (!mult) {
        r.error = "bad multiplier band";
        return r;
    }
    r.ohms = value * *mult;
    r.tolerance = 0.20;
    if (n >= 4) {
        const auto tol = band_tolerance(b[digits + 1]);
        if (!tol) {
            r.error = std::string(color_name(b[digits + 1])) + " is not a tolerance colour";
            return r;
        }
        r.tolerance = *tol;
    }
    if (n == 6) {
        const auto tc = band_tempco(b[5]);
        if (!tc) {
            r.error = "bad temperature band";
            return r;
        }
        r.tempco = *tc;
    }
    r.ok = true;
    return r;
}

}  // namespace

BandReading decode_bands(const std::vector<Color>& bands) {
    BandReading r = decode_one_way(bands);
    if (r.ok || bands.size() < 3) return r;
    std::vector<Color> rev(bands.rbegin(), bands.rend());
    BandReading back = decode_one_way(rev);
    if (back.ok && rev[0] != Color::Black) {  // a leading black band is never the first digit
        back.reversed = true;
        return back;
    }
    return r;
}

std::vector<Color> encode_bands(double ohms, int digits, double tolerance) {
    std::vector<Color> out;
    if (!(ohms > 0) || !std::isfinite(ohms)) return out;
    const int e = static_cast<int>(std::floor(std::log10(ohms) + 1e-9)) - (digits - 1);
    if (e < -2 || e > 9) return out;
    const double m = std::round(ohms / std::pow(10.0, e));
    if (std::fabs(m * std::pow(10.0, e) / ohms - 1) > 1e-6) return out;  // needs more digits
    int mi = static_cast<int>(m);
    std::vector<Color> ds;
    for (int i = 0; i < digits; ++i) {
        ds.insert(ds.begin(), static_cast<Color>(mi % 10));
        mi /= 10;
    }
    out = ds;
    out.push_back(e == -1 ? Color::Gold : e == -2 ? Color::Silver : static_cast<Color>(e));
    if (tolerance < 0.2 - 1e-9) {
        for (Color c : {Color::Brown, Color::Red, Color::Orange, Color::Yellow, Color::Green, Color::Blue,
                        Color::Violet, Color::Grey, Color::Gold, Color::Silver}) {
            if (std::fabs(*band_tolerance(c) - tolerance) < 1e-9) {
                out.push_back(c);
                break;
            }
        }
    }
    return out;
}

// ---------------------------------------------------------------- printed codes

std::vector<Reading> decode_marking(const std::string& input) {
    std::string s;
    for (char c : input) {
        if (c != ' ') s += c;
    }
    std::vector<Reading> out;
    if (s.empty()) return out;
    const std::string U = upper(s);

    for (const auto& p : kParts) {
        if (U == p.code) out.push_back({p.kind, "part code (check the package)", p.text});
    }
    const bool part = !out.empty();

    // zero-ohm links
    if (U == "0" || U == "00" || U == "000" || U == "0000" || U == "0R" || U == "0R0" || U == "R000") {
        out.push_back({'R', "jumper", "0 \xCE\xA9 link"});
        return out;
    }

    if (all_digits(s)) {
        if (s.size() == 3) {
            const int m = s[2] - '0';
            const double base = std::stod(s.substr(0, 2));
            if (m <= 7) out.push_back({'R', "3-digit SMD (\xC2\xB1" "5 %)", ohms_text(base * std::pow(10.0, m)), base * std::pow(10.0, m)});
            if (const auto pf = cap_code_pf(s)) out.push_back({'C', "3-digit code (pF)", farads_text(*pf * 1e-12)});
            if (m <= 6) out.push_back({'L', "3-digit code (\xC2\xB5H)", henries_text(base * std::pow(10.0, m) * 1e-6)});
        } else if (s.size() == 4) {
            const int m = s[3] - '0';
            const double base = std::stod(s.substr(0, 3));
            if (m <= 7) out.push_back({'R', "4-digit SMD (\xC2\xB1" "1 %)", ohms_text(base * std::pow(10.0, m)), base * std::pow(10.0, m)});
        } else if (s.size() <= 2) {
            const double v = std::stod(s);
            out.push_back({'R', "plain value", ohms_text(v), v});
            out.push_back({'C', "plain value (pF)", farads_text(v * 1e-12)});
        }
        return out;
    }

    // capacitor: [voltage code] 3 digits [tolerance letter]  -  "104K", "2A104J", "1H473"
    bool cap_code = false;
    {
        std::string body = U;
        std::optional<double> volts;
        if (body.size() >= 5 && std::isdigit(static_cast<unsigned char>(body[0])) && std::isalpha(static_cast<unsigned char>(body[1]))) {
            volts = voltage_code(body.substr(0, 2));
            if (volts) body = body.substr(2);
        }
        std::optional<double> tol;
        if (body.size() == 4 && std::isalpha(static_cast<unsigned char>(body[3]))) {
            tol = tolerance_letter(body[3]);
            if (tol) body.pop_back();
        }
        if ((volts || tol) && body.size() == 3 && all_digits(body)) {
            cap_code = true;
            if (const auto pf = cap_code_pf(body)) {
                std::string v = farads_text(*pf * 1e-12);
                if (tol) v += "  " + percent(*tol);
                if (volts) v += "  " + format_plain(*volts, 3) + " V";
                out.push_back({'C', volts ? "voltage + 3-digit code" : "3-digit + tolerance", v});
            }
        }
    }

    // tantalum: voltage letter + 3-digit pF code  -  "A106" = 10 µF 10 V
    if (U.size() == 4 && std::isalpha(static_cast<unsigned char>(U[0])) && all_digits(U.substr(1))) {
        const auto v = tantalum_voltage(U[0]);
        const auto pf = cap_code_pf(U.substr(1));
        if (v && pf) {
            out.push_back({'C', "tantalum (letter = voltage)", farads_text(*pf * 1e-12) + "  " + format_plain(*v, 3) + " V"});
        }
    }

    // RKM ("4R7", "4K7", "1M0", "4n7", "u47"), optionally with a tolerance letter ("4K7J")
    if (!cap_code && !part && !decode_rkm(s, out) && s.size() >= 3 && tolerance_letter(s.back())) {
        const size_t before = out.size();
        if (decode_rkm(s.substr(0, s.size() - 1), out)) {
            for (size_t i = before; i < out.size(); ++i) out[i].value += "  " + percent(*tolerance_letter(s.back()));
        }
    }

    // EIA-96: 2 digits (index into E96) + multiplier letter
    if (U.size() == 3 && std::isdigit(static_cast<unsigned char>(U[0])) &&
        std::isdigit(static_cast<unsigned char>(U[1]))) {
        const int idx = (U[0] - '0') * 10 + (U[1] - '0');
        double mult = 0;
        switch (U[2]) {
            case 'Z': mult = 0.001; break;
            case 'Y':
            case 'R': mult = 0.01; break;
            case 'X':
            case 'S': mult = 0.1; break;
            case 'A': mult = 1; break;
            case 'B':
            case 'H': mult = 10; break;
            case 'C': mult = 100; break;
            case 'D': mult = 1e3; break;
            case 'E': mult = 1e4; break;
            case 'F': mult = 1e5; break;
            default: break;
        }
        if (idx >= 1 && idx <= 96 && mult > 0) {
            out.push_back({'R', "EIA-96 SMD (\xC2\xB1" "1 %)", ohms_text(e96()[static_cast<size_t>(idx - 1)] * mult), e96()[static_cast<size_t>(idx - 1)] * mult});
        }
    }

    // a plain value with a prefix / unit: "4.7k", "100nF", "10uH", "2.2u"
    if (out.empty()) {
        if (const auto v = parse_si(s)) {
            const char last = s.back();
            const bool farad = last == 'F' || s.find('p') != std::string::npos || s.find('n') != std::string::npos;
            const bool henry = last == 'H';
            if (henry) {
                out.push_back({'L', "value", henries_text(*v)});
            } else if (farad) {
                out.push_back({'C', "value", farads_text(*v)});
            } else if (s.find('u') != std::string::npos || s.find("\xC2\xB5") != std::string::npos) {
                out.push_back({'C', "value", farads_text(*v)});
                out.push_back({'L', "value", henries_text(*v)});
            } else {
                out.push_back({'R', "value", ohms_text(*v), *v});
            }
        }
    }
    return out;
}

}  // namespace ohm
