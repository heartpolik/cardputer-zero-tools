#include "engine.h"

#include <chrono>
#include <ctime>

namespace ser_ui {
namespace mb = ser::modbus;

namespace {

std::string wall_clock(bool for_file) {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const std::time_t t = system_clock::to_time_t(now);
    const int ms = static_cast<int>(duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000);
    std::tm tm{};
    localtime_r(&t, &tm);
    char buf[40];
    if (for_file) std::strftime(buf, sizeof buf, "%Y%m%d-%H%M%S", &tm);
    else {
        char hms[16];
        std::strftime(hms, sizeof hms, "%H:%M:%S", &tm);
        std::snprintf(buf, sizeof buf, "%s.%03d", hms, ms);
    }
    return buf;
}

}  // namespace

Engine::Engine(ser::Session& session, std::filesystem::path log_dir) : s_(session), log_dir_(std::move(log_dir)) {
    framer_.configure(s_.framer());
}

Engine::~Engine() { close(); }

bool Engine::open() {
    close();
    error_.clear();
    port_ = open_port(s_.port, s_.fmt, error_);
    framer_.configure(s_.framer());
    sniffer_.reset();
    return port_ != nullptr;
}

void Engine::close() {
    if (!port_) return;
    std::vector<ser::Frame> rest;
    framer_.flush(rest);
    for (auto& f : rest) on_frame(std::move(f));
    port_.reset();
    master_.cancel();
    polling_ = false;
    scanner_.stop();
    ++mb_serial_;
}

void Engine::apply_format() {
    framer_.configure(s_.framer());
    if (!port_) return;
    std::string err;
    if (!port_->set_format(s_.fmt, err)) {
        close();  // never keep running with a frame other than the one shown
        error_ = err;
    }
}

void Engine::clear_capture() {
    capture_.clear();
    sniffer_.reset();
}

bool Engine::send(const ser::Bytes& bytes) {
    if (!port_) {
        error_ = "port is closed";
        return false;
    }
    std::string err;
    if (!port_->write(bytes, err)) {
        error_ = err;
        return false;
    }
    ser::Frame f{ser::Dir::Tx, now_us(), bytes, {}};
    if (s_.modbus_labels && mb::crc_ok(bytes)) f.note = sniffer_.describe(bytes).text;
    record(std::move(f));
    return true;
}

void Engine::record(ser::Frame f) {
    if (log_.is_open()) {
        log_ << ser::log_line(f, wall_clock(false));
        if (!f.note.empty()) log_ << "  ; " << f.note;
        log_ << '\n';
        log_.flush();
    }
    capture_.add(std::move(f));
}

void Engine::on_frame(ser::Frame f) {
    std::vector<ser::Bytes> parts;
    if (s_.modbus_labels && f.bytes.size() >= 8 && !mb::crc_ok(f.bytes)) parts = mb::split_frames(f.bytes);
    if (parts.size() <= 1) parts = {f.bytes};
    for (auto& p : parts) {
        ser::Frame one{ser::Dir::Rx, f.t_us, p, {}};
        if (s_.modbus_labels && mb::crc_ok(p)) one.note = sniffer_.describe(p).text;
        record(std::move(one));
        modbus_event(master_.on_frame(p, now_us()));
    }
}

bool Engine::start_request(const mb::Request& r) {
    if (!port_ || master_.busy()) return false;
    master_.timeout_ms = s_.timeout_ms;
    const ser::Bytes q = master_.start(r, now_us());
    if (!send(q)) {
        master_.cancel();
        ++mb_serial_;
        return false;
    }
    ++mb_serial_;
    return true;
}

void Engine::modbus_event(mb::Event ev) {
    if (ev == mb::Event::None) return;
    ++mb_serial_;
    if (scanner_.running()) {
        scanner_.advance(ev, master_.last());
        return;
    }
    if (ev == mb::Event::Ok) {
        table_req_ = master_.request();
        table_ = master_.last();
        has_table_ = true;
    }
}

void Engine::read_once() {
    if (scanner_.running()) return;
    if (!port_) {
        error_ = "port is closed";
        ++mb_serial_;
        return;
    }
    start_request(s_.req);
}

void Engine::set_polling(bool on) {
    polling_ = on && port_ != nullptr;
    next_poll_us_ = 0;
    if (polling_) scanner_.stop();
    ++mb_serial_;
}

void Engine::start_scan() {
    if (!port_) {
        error_ = "port is closed";
        ++mb_serial_;
        return;
    }
    polling_ = false;
    master_.cancel();
    mb::Request probe = s_.req;
    probe.count = 1;
    scanner_.start(probe, s_.scan_first, s_.scan_last);
    ++mb_serial_;
}

void Engine::stop_scan() {
    scanner_.stop();
    master_.cancel();
    ++mb_serial_;
}

bool Engine::set_logging(bool on) {
    if (!on) {
        log_.close();
        return true;
    }
    if (log_.is_open()) return true;
    std::error_code ec;
    std::filesystem::create_directories(log_dir_, ec);
    log_path_ = (log_dir_ / ("serial-" + wall_clock(true) + ".log")).string();
    log_.open(log_path_, std::ios::app);
    if (!log_) {
        error_ = "cannot write " + log_path_;
        return false;
    }
    log_ << "# " << s_.port << ' ' << s_.fmt.baud << ' ' << ser::frame_name(s_.fmt) << '\n';
    return true;
}

void Engine::tick() {
    const uint64_t now = now_us();
    if (port_) {
        std::vector<Chunk> chunks;
        port_->drain(chunks);
        std::vector<ser::Frame> frames;
        for (const auto& c : chunks) framer_.feed(c.bytes.data(), c.bytes.size(), c.t_us, frames);
        framer_.idle(now, frames);
        for (auto& f : frames) on_frame(std::move(f));
        const std::string fail = port_->failure();
        if (!fail.empty()) {
            close();
            error_ = fail;
            return;
        }
    }
    modbus_event(master_.on_tick(now));

    if (!port_ || master_.busy()) return;
    if (scanner_.running()) {
        start_request(scanner_.probe());
    } else if (polling_ && now >= next_poll_us_) {
        next_poll_us_ = now + static_cast<uint64_t>(s_.poll_ms) * 1000;
        start_request(s_.req);
    }
}

}  // namespace ser_ui
