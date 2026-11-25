// Ties the port, framing, capture, Modbus master/scanner and the log file
// together. Driven by tick() from the UI loop; no LVGL here.
#pragma once

#include "capture.h"
#include "modbus.h"
#include "serial_port.h"
#include "session.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace ser_ui {

class Engine {
public:
    Engine(ser::Session& session, std::filesystem::path log_dir);
    ~Engine();

    // ---- port
    bool open();   // session.port with session.fmt; error() says why not
    void close();
    bool is_open() const { return port_ != nullptr; }
    const std::string& error() const { return error_; }
    void apply_format();  // after baud / frame / split changes

    // ---- terminal
    bool send(const ser::Bytes& bytes);
    ser::Capture& capture() { return capture_; }
    void clear_capture();

    // ---- Modbus
    void read_once();
    bool polling() const { return polling_; }
    void set_polling(bool on);
    void start_scan();
    void stop_scan();
    bool scanning() const { return scanner_.running(); }
    const ser::modbus::Master& master() const { return master_; }
    const ser::modbus::Scanner& scanner() const { return scanner_; }
    void reset_stats() { master_.reset_stats(); }
    // the last good answer and what it answered
    const ser::modbus::Request& table_request() const { return table_req_; }
    const ser::modbus::Response& table() const { return table_; }
    bool has_table() const { return has_table_; }
    // changes whenever Modbus state changed (to refresh views)
    uint64_t modbus_serial() const { return mb_serial_; }

    // ---- log file
    bool logging() const { return log_.is_open(); }
    const std::string& log_path() const { return log_path_; }
    bool set_logging(bool on);

    void tick();

private:
    void on_frame(ser::Frame f);
    void record(ser::Frame f);
    void modbus_event(ser::modbus::Event ev);
    bool start_request(const ser::modbus::Request& r);

    ser::Session& s_;
    std::filesystem::path log_dir_;
    std::unique_ptr<Port> port_;
    std::string error_;
    ser::Framer framer_;
    ser::Capture capture_;
    ser::modbus::Sniffer sniffer_;
    ser::modbus::Master master_;
    ser::modbus::Scanner scanner_;
    bool polling_{false};
    uint64_t next_poll_us_{0};
    ser::modbus::Request table_req_;
    ser::modbus::Response table_;
    bool has_table_{false};
    uint64_t mb_serial_{0};
    std::ofstream log_;
    std::string log_path_;
};

}  // namespace ser_ui
