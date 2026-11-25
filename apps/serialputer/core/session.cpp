#include "session.h"

#include "bytes.h"

#include <cstdlib>
#include <sstream>

namespace ser {
namespace {

std::vector<std::string> split_tabs(const std::string& line, size_t max_fields) {
    std::vector<std::string> f;
    size_t start = 0;
    while (f.size() + 1 < max_fields) {
        const size_t tab = line.find('\t', start);
        if (tab == std::string::npos) break;
        f.push_back(line.substr(start, tab - start));
        start = tab + 1;
    }
    f.push_back(line.substr(start));
    return f;
}

int to_int(const std::string& s) { return std::atoi(s.c_str()); }

int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

}  // namespace

const char* line_end_name(LineEnd e) {
    switch (e) {
        case LineEnd::None: return "none";
        case LineEnd::CR: return "CR";
        case LineEnd::LF: return "LF";
        case LineEnd::CRLF: return "CRLF";
    }
    return "";
}

LineEnd next_line_end(LineEnd e) { return static_cast<LineEnd>((static_cast<int>(e) + 1) % 4); }

SendResult prepare_send(const std::string& input, SendMode mode, LineEnd end, bool append_crc) {
    SendResult r;
    if (mode == SendMode::Hex) {
        const auto p = cz::parse_bytes(input);
        if (!p.ok) {
            r.error = p.error;
            return r;
        }
        r.bytes = p.bytes;
        if (append_crc && !r.bytes.empty()) modbus::append_crc(r.bytes);
        r.ok = !r.bytes.empty();
        if (!r.ok) r.error = "nothing to send";
        return r;
    }
    for (size_t i = 0; i < input.size(); ++i) {
        char c = input[i];
        if (c == '\\' && i + 1 < input.size()) {
            const char e = input[++i];
            if (e == 'x') {
                const int h = i + 1 < input.size() ? hex_digit(input[i + 1]) : -1;
                const int l = i + 2 < input.size() ? hex_digit(input[i + 2]) : -1;
                if (h < 0 || l < 0) {
                    r.error = "\\x needs two hex digits";
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
    if (end == LineEnd::CR || end == LineEnd::CRLF) r.bytes.push_back('\r');
    if (end == LineEnd::LF || end == LineEnd::CRLF) r.bytes.push_back('\n');
    r.ok = !r.bytes.empty();
    if (!r.ok) r.error = "nothing to send";
    return r;
}

void Session::remember(const std::string& input) {
    if (input.empty()) return;
    if (!history.empty() && history.back() == input) return;
    history.push_back(input);
    if (history.size() > kMaxHistory) history.erase(history.begin());
}

FramerConfig Session::framer() const {
    FramerConfig c;
    c.split = split;
    c.char_us = char_time_us(fmt);
    // Linux delivers bytes in bursts: never trust a gap shorter than 2 ms
    const uint32_t auto_gap = modbus_gap_us(fmt) < 2000 ? 2000 : modbus_gap_us(fmt);
    c.gap_us = gap_ms ? gap_ms * 1000 : auto_gap;
    return c;
}

std::string Session::serialize() const {
    std::ostringstream o;
    o << "cz-serial\t1\n";
    o << "port\t" << port << '\n';
    o << "baud\t" << fmt.baud << '\n';
    o << "frame\t" << frame_name(fmt) << '\n';
    o << "split\t" << (split == Split::Line ? "line" : "gap") << '\t' << gap_ms << '\n';
    o << "view\t" << static_cast<int>(view) << '\n';
    o << "time\t" << static_cast<int>(time) << '\n';
    o << "send\t" << (send == SendMode::Hex ? "hex" : "text") << '\t' << static_cast<int>(line_end) << '\t'
      << (append_crc ? 1 : 0) << '\n';
    o << "labels\t" << (modbus_labels ? 1 : 0) << '\n';
    o << "mb\t" << int(req.addr) << '\t' << int(req.fc) << '\t' << req.start << '\t' << req.count << '\n';
    o << "interp\t" << static_cast<int>(interp) << '\n';
    o << "poll\t" << poll_ms << '\t' << timeout_ms << '\n';
    o << "scan\t" << int(scan_first) << '\t' << int(scan_last) << '\n';
    for (const auto& h : history) o << "hist\t" << h << '\n';
    return o.str();
}

void Session::deserialize(const std::string& text) {
    std::istringstream in(text);
    std::string line;
    if (!std::getline(in, line) || line.rfind("cz-serial\t", 0) != 0) return;
    while (std::getline(in, line)) {
        const auto f = split_tabs(line, 5);
        const std::string& k = f[0];
        if (k == "port" && f.size() >= 2 && !f[1].empty()) {
            port = f[1];
        } else if (k == "baud" && f.size() >= 2) {
            const int b = to_int(f[1]);
            if (b >= 50 && b <= 4000000) fmt.baud = b;
        } else if (k == "frame" && f.size() >= 2) {
            parse_frame(f[1], fmt);
        } else if (k == "split" && f.size() >= 3) {
            split = f[1] == "line" ? Split::Line : Split::Gap;
            gap_ms = static_cast<uint32_t>(to_int(f[2]));
        } else if (k == "view" && f.size() >= 2) {
            const int v = to_int(f[1]);
            if (v >= 0 && v <= 2) view = static_cast<View>(v);
        } else if (k == "time" && f.size() >= 2) {
            const int v = to_int(f[1]);
            if (v >= 0 && v <= 2) time = static_cast<TimeMode>(v);
        } else if (k == "send" && f.size() >= 4) {
            send = f[1] == "hex" ? SendMode::Hex : SendMode::Text;
            const int e = to_int(f[2]);
            if (e >= 0 && e <= 3) line_end = static_cast<LineEnd>(e);
            append_crc = f[3] == "1";
        } else if (k == "labels" && f.size() >= 2) {
            modbus_labels = f[1] == "1";
        } else if (k == "mb" && f.size() >= 5) {
            const int a = to_int(f[1]), fc = to_int(f[2]), st = to_int(f[3]), n = to_int(f[4]);
            if (a >= 0 && a <= 247) req.addr = static_cast<uint8_t>(a);
            if (modbus::is_read(static_cast<uint8_t>(fc))) req.fc = static_cast<uint8_t>(fc);
            if (st >= 0 && st <= 65535) req.start = static_cast<uint16_t>(st);
            if (n >= 1 && n <= modbus::max_count(req.fc)) req.count = static_cast<uint16_t>(n);
        } else if (k == "interp" && f.size() >= 2) {
            const int v = to_int(f[1]);
            if (v >= 0 && v < 8) interp = static_cast<modbus::Interp>(v);
        } else if (k == "poll" && f.size() >= 3) {
            const int p = to_int(f[1]), t = to_int(f[2]);
            if (p >= 100 && p <= 60000) poll_ms = static_cast<uint32_t>(p);
            if (t >= 20 && t <= 10000) timeout_ms = static_cast<uint32_t>(t);
        } else if (k == "scan" && f.size() >= 3) {
            const int a = to_int(f[1]), b = to_int(f[2]);
            if (a >= 1 && a <= 247 && b >= a && b <= 247) {
                scan_first = static_cast<uint8_t>(a);
                scan_last = static_cast<uint8_t>(b);
            }
        } else if (k == "hist") {
            history.push_back(line.size() > 5 ? line.substr(5) : "");
        }
    }
    if (history.size() > kMaxHistory) history.erase(history.begin(), history.end() - kMaxHistory);
}

}  // namespace ser
