#include "capture.h"

#include <algorithm>
#include <cstdio>

namespace ser {

// ---------------------------------------------------------------- Framer

void Framer::close(std::vector<Frame>& out) {
    if (cur_.bytes.empty()) return;
    out.push_back(std::move(cur_));
    cur_ = Frame{};
}

void Framer::feed(const uint8_t* data, size_t n, uint64_t t_us, std::vector<Frame>& out) {
    if (n == 0) return;
    // the chunk's first byte finished arriving (n-1) characters before its last one
    const uint64_t span = static_cast<uint64_t>(cfg_.char_us) * (n - 1);
    const uint64_t first_us = t_us > span ? t_us - span : 0;
    if (pending() && first_us > last_us_ && first_us - last_us_ > quiet_us() + cfg_.char_us) close(out);

    for (size_t i = 0; i < n; ++i) {
        if (cur_.bytes.empty()) {
            cur_.dir = Dir::Rx;
            const uint64_t byte_end = first_us + static_cast<uint64_t>(cfg_.char_us) * i;
            cur_.t_us = byte_end > cfg_.char_us ? byte_end - cfg_.char_us : 0;
        }
        cur_.bytes.push_back(data[i]);
        const bool line_end = cfg_.split == Split::Line && data[i] == '\n';
        if (line_end || cur_.bytes.size() >= cfg_.max_frame) close(out);
    }
    last_us_ = t_us;
}

void Framer::idle(uint64_t now_us, std::vector<Frame>& out) {
    if (pending() && now_us > last_us_ && now_us - last_us_ > quiet_us()) close(out);
}

void Framer::flush(std::vector<Frame>& out) { close(out); }

// ---------------------------------------------------------------- Capture

void Capture::add(Frame f) {
    if (!started_) {
        start_us_ = f.t_us;
        started_ = true;
    }
    (f.dir == Dir::Rx ? rx_bytes_ : tx_bytes_) += f.bytes.size();
    frames_.push_back(std::move(f));
    while (frames_.size() > max_) frames_.pop_front();
    ++serial_;
}

void Capture::clear() {
    frames_.clear();
    rx_bytes_ = tx_bytes_ = 0;
    started_ = false;
    start_us_ = 0;
    ++serial_;
}

// ---------------------------------------------------------------- views

const char* view_name(View v) {
    switch (v) {
        case View::Hex: return "HEX";
        case View::Text: return "TXT";
        case View::Dump: return "DUMP";
    }
    return "";
}

View next_view(View v) {
    switch (v) {
        case View::Hex: return View::Text;
        case View::Text: return View::Dump;
        case View::Dump: return View::Hex;
    }
    return View::Hex;
}

const char* time_mode_name(TimeMode t) {
    switch (t) {
        case TimeMode::Off: return "OFF";
        case TimeMode::Rel: return "REL";
        case TimeMode::Delta: return "DELTA";
    }
    return "";
}

TimeMode next_time_mode(TimeMode t) {
    switch (t) {
        case TimeMode::Off: return TimeMode::Rel;
        case TimeMode::Rel: return TimeMode::Delta;
        case TimeMode::Delta: return TimeMode::Off;
    }
    return TimeMode::Off;
}

std::string hex_string(const Bytes& b, size_t from, size_t to) {
    to = std::min(to, b.size());
    std::string s;
    char buf[4];
    for (size_t i = from; i < to; ++i) {
        std::snprintf(buf, sizeof buf, "%02X", b[i]);
        if (i > from) s += ' ';
        s += buf;
    }
    return s;
}

namespace {

std::string escape(uint8_t c) {
    switch (c) {
        case '\r': return "\\r";
        case '\n': return "\\n";
        case '\t': return "\\t";
        case 0: return "\\0";
        case '\\': return "\\\\";
        default: break;
    }
    if (c >= 32 && c < 127) return std::string(1, static_cast<char>(c));
    char buf[5];
    std::snprintf(buf, sizeof buf, "\\x%02X", c);
    return buf;
}

}  // namespace

std::string text_string(const Bytes& b, size_t from, size_t to) {
    to = std::min(to, b.size());
    std::string s;
    for (size_t i = from; i < to; ++i) s += escape(b[i]);
    return s;
}

std::vector<std::string> wrap_frame(const Frame& f, View v, int cols) {
    std::vector<std::string> lines;
    cols = std::max(cols, 8);
    const size_t n = f.bytes.size();
    if (n == 0) return {""};

    if (v == View::Hex) {
        const size_t per = static_cast<size_t>((cols + 1) / 3);
        for (size_t i = 0; i < n; i += per) lines.push_back(hex_string(f.bytes, i, i + per));
        return lines;
    }
    if (v == View::Dump) {
        // "01 03 0A  ..." : 3 per byte + 1 space + 1 per byte
        const size_t per = static_cast<size_t>(std::max(1, (cols - 1) / 4));
        for (size_t i = 0; i < n; i += per) {
            std::string hex = hex_string(f.bytes, i, i + per);
            hex.resize(per * 3 - 1, ' ');
            std::string ascii;
            for (size_t j = i; j < std::min(n, i + per); ++j) {
                const uint8_t c = f.bytes[j];
                ascii += (c >= 32 && c < 127) ? static_cast<char>(c) : '.';
            }
            lines.push_back(hex + "  " + ascii);
        }
        return lines;
    }
    // Text: escapes are never split across lines; a trailing \n ends the frame
    std::string line;
    for (size_t i = 0; i < n; ++i) {
        const std::string e = escape(f.bytes[i]);
        if (static_cast<int>(line.size() + e.size()) > cols) {
            lines.push_back(line);
            line.clear();
        }
        line += e;
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

std::string time_label(TimeMode mode, uint64_t t_us, uint64_t start_us, uint64_t prev_us) {
    char buf[24];
    switch (mode) {
        case TimeMode::Off:
            return "";
        case TimeMode::Rel: {
            const uint64_t d = t_us > start_us ? t_us - start_us : 0;
            const uint64_t ms = d / 1000;
            std::snprintf(buf, sizeof buf, "%llu.%03llu", static_cast<unsigned long long>(ms / 1000),
                          static_cast<unsigned long long>(ms % 1000));
            return buf;
        }
        case TimeMode::Delta: {
            const uint64_t d = t_us > prev_us ? t_us - prev_us : 0;
            if (d < 10000000) {
                std::snprintf(buf, sizeof buf, "+%llums", static_cast<unsigned long long>(d / 1000));
            } else {
                std::snprintf(buf, sizeof buf, "+%.1fs", static_cast<double>(d) / 1e6);
            }
            return buf;
        }
    }
    return "";
}

std::string log_line(const Frame& f, const std::string& prefix) {
    std::string s = prefix;
    if (!s.empty()) s += ' ';
    s += f.dir == Dir::Rx ? "RX " : "TX ";
    s += hex_string(f.bytes);
    s += "  |";
    s += text_string(f.bytes);
    s += '|';
    return s;
}

}  // namespace ser
