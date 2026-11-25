// Serial ports: a real tty (termios, reader thread with timestamps) or the
// simulated bus. No LVGL here.
#pragma once

#include "capture.h"
#include "serial_format.h"

#include <memory>
#include <string>
#include <vector>

namespace ser_ui {

uint64_t now_us();  // monotonic

struct Chunk {
    uint64_t t_us;  // when the last byte was read
    ser::Bytes bytes;
};

struct PortInfo {
    std::string path;    // "/dev/ttyAMA0", "SIM"
    std::string target;  // for symlinks such as /dev/serial0: the real device
    bool console{false};      // the kernel console or a login shell may use it
    bool accessible{true};    // we may open it read/write
};

// Ports that exist now (SIM first). Linux: ttyAMA*, ttyS*, ttyUSB*, ttyACM*, serial0/1;
// macOS: /dev/cu.*
std::vector<PortInfo> list_ports();
PortInfo describe_port(const std::string& path);

class Port {
public:
    virtual ~Port() = default;
    virtual bool write(const ser::Bytes& bytes, std::string& error) = 0;
    // everything read since the last call
    virtual void drain(std::vector<Chunk>& out) = 0;
    virtual bool set_format(const ser::SerialFormat& fmt, std::string& error) = 0;
    // a read error that closed the port (cable pulled, USB adapter gone)
    virtual std::string failure() const { return ""; }
};

std::unique_ptr<Port> open_port(const std::string& path, const ser::SerialFormat& fmt, std::string& error);

}  // namespace ser_ui
