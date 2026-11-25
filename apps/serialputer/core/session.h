// SerialPuter settings that survive restarts, and turning typed input into
// bytes to send (pure C++).
#pragma once

#include "capture.h"
#include "modbus.h"
#include "serial_format.h"

#include <string>
#include <vector>

namespace ser {

constexpr const char* kSimPort = "SIM";

enum class LineEnd { None, CR, LF, CRLF };
const char* line_end_name(LineEnd e);
LineEnd next_line_end(LineEnd e);

enum class SendMode { Text, Hex };  // Text: typed characters (+ line end); Hex: 01 03 ... or "text"

struct SendResult {
    bool ok{false};
    Bytes bytes;
    std::string error;
};

// Text mode understands escapes (\r \n \t \xHH); Hex mode is cz::parse_bytes.
// append_crc adds the Modbus CRC (Hex mode only).
SendResult prepare_send(const std::string& input, SendMode mode, LineEnd end, bool append_crc);

struct Session {
    static constexpr size_t kMaxHistory = 50;

    std::string port{kSimPort};
    SerialFormat fmt;
    Split split{Split::Gap};
    uint32_t gap_ms{0};  // 0: automatic (Modbus 3.5 characters)
    View view{View::Hex};
    TimeMode time{TimeMode::Rel};
    SendMode send{SendMode::Text};
    LineEnd line_end{LineEnd::CRLF};
    bool append_crc{false};
    bool modbus_labels{true};  // describe Modbus frames in the terminal
    std::vector<std::string> history;

    modbus::Request req;
    modbus::Interp interp{modbus::Interp::U16};
    uint32_t poll_ms{1000};
    uint32_t timeout_ms{300};
    uint8_t scan_first{1}, scan_last{247};

    void remember(const std::string& input);
    FramerConfig framer() const;

    std::string serialize() const;
    void deserialize(const std::string& text);
};

}  // namespace ser
