// Splitting a byte stream into frames, the capture log and how frames are shown
// (pure C++).
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace ser {

using Bytes = std::vector<uint8_t>;

enum class Dir { Rx, Tx };

struct Frame {
    Dir dir{Dir::Rx};
    uint64_t t_us{0};  // when the frame started (Rx: estimated first byte)
    Bytes bytes;
    std::string note;  // protocol label ("#1 READ HOLDING 0 x10"), may be empty
};

// Gap: a pause longer than gap_us ends a frame (binary protocols, Modbus).
// Line: '\n' ends a frame; a pause of line_idle_us flushes a prompt without one.
enum class Split { Gap, Line };

struct FramerConfig {
    Split split{Split::Gap};
    uint32_t gap_us{4000};
    uint32_t char_us{1042};        // one character on the wire (9600 8N1)
    uint32_t line_idle_us{150000};
    size_t max_frame{256};
};

class Framer {
public:
    void configure(const FramerConfig& cfg) { cfg_ = cfg; }
    const FramerConfig& config() const { return cfg_; }

    // A chunk read at t_us (arrival of its last byte). Completed frames go to out.
    void feed(const uint8_t* data, size_t n, uint64_t t_us, std::vector<Frame>& out);
    // Ends the pending frame once the line has been quiet long enough.
    void idle(uint64_t now_us, std::vector<Frame>& out);
    void flush(std::vector<Frame>& out);
    bool pending() const { return !cur_.bytes.empty(); }

private:
    void close(std::vector<Frame>& out);
    uint32_t quiet_us() const { return cfg_.split == Split::Gap ? cfg_.gap_us : cfg_.line_idle_us; }

    FramerConfig cfg_;
    Frame cur_;
    uint64_t last_us_{0};  // arrival of the last byte of cur_
};

// What the terminal keeps: the last frames plus running counters.
class Capture {
public:
    explicit Capture(size_t max_frames = 400) : max_(max_frames) {}

    void add(Frame f);
    void clear();
    const std::deque<Frame>& frames() const { return frames_; }
    uint64_t rx_bytes() const { return rx_bytes_; }
    uint64_t tx_bytes() const { return tx_bytes_; }
    uint64_t start_us() const { return start_us_; }
    // increases whenever a frame is added; lets views notice new data
    uint64_t serial() const { return serial_; }

private:
    size_t max_;
    std::deque<Frame> frames_;
    uint64_t rx_bytes_{0}, tx_bytes_{0};
    uint64_t start_us_{0};
    bool started_{false};
    uint64_t serial_{0};
};

enum class View { Hex, Text, Dump };  // Dump: hex + ASCII side by side
const char* view_name(View v);
View next_view(View v);

enum class TimeMode { Off, Rel, Delta };  // Rel: seconds since capture start; Delta: ms since previous frame
const char* time_mode_name(TimeMode t);
TimeMode next_time_mode(TimeMode t);

std::string hex_string(const Bytes& b, size_t from = 0, size_t to = SIZE_MAX);
// printable ASCII as is, \r \n \t \0 as escapes, anything else \xHH
std::string text_string(const Bytes& b, size_t from = 0, size_t to = SIZE_MAX);

// A frame laid out in lines of at most cols characters.
std::vector<std::string> wrap_frame(const Frame& f, View v, int cols);

// "12.345" (Rel) or "+12ms" / "+1.20s" (Delta); empty for Off
std::string time_label(TimeMode mode, uint64_t t_us, uint64_t start_us, uint64_t prev_us);

// One line for a log file: "<prefix> RX 01 03 00 0A  |..|"
std::string log_line(const Frame& f, const std::string& prefix);

}  // namespace ser
