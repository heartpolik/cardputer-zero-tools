#include "port_page.h"

#include <algorithm>
#include <cstdio>

namespace ser_ui {
namespace {

namespace key = platform::key;

constexpr int kRowY = 2;
constexpr int kRowH = 14;
constexpr int kInfoY = 131;

constexpr const char* kNames[PortPage::kRows] = {"Port",       "Baud",    "Frame", "Split", "Modbus labels",
                                                 "Poll every", "Timeout", "Scan",  "Log file"};

// split settings: gap_ms (0 = auto), -1 = line mode
const std::vector<int> kSplits = {0, 5, 10, 20, 50, 100, -1};
const std::vector<uint32_t> kPolls = {100, 200, 500, 1000, 2000, 5000, 10000};
const std::vector<uint32_t> kTimeouts = {50, 100, 200, 300, 500, 1000, 2000};
const std::vector<int> kScanLast = {247, 32, 16, 8};

template <class T>
T cycle(const std::vector<T>& v, const T& cur, int dir) {
    const auto it = std::find(v.begin(), v.end(), cur);
    const int n = static_cast<int>(v.size());
    const int i = it == v.end() ? 0 : static_cast<int>(it - v.begin());
    return v[static_cast<size_t>((i + dir + n) % n)];
}

std::string short_port(const std::string& p) {
    const auto slash = p.rfind('/');
    return slash == std::string::npos ? p : p.substr(slash + 1);
}

}  // namespace

void PortPage::build(lv_obj_t* parent) {
    const auto& s = ctx_.style;
    for (int i = 0; i < kRows; ++i) {
        rows_[i] = make_box(parent, 4, kRowY + i * kRowH, kWidth - 8, kRowH);
        lv_obj_set_style_bg_color(rows_[i], s.selection, 0);
        lv_obj_set_style_radius(rows_[i], 2, 0);
        names_[i] = make_label(rows_[i], s.mono12, s.dim, kNames[i]);
        lv_obj_set_pos(names_[i], 4, -1);
        values_[i] = make_label(rows_[i], s.mono12, s.text);
        lv_obj_set_pos(values_[i], 116, -1);
        lv_obj_set_width(values_[i], kWidth - 8 - 120);
        lv_label_set_long_mode(values_[i], LV_LABEL_LONG_MODE_DOTS);
    }
    info_ = make_label(parent, s.ui10, s.dim);
    lv_obj_set_pos(info_, 8, kInfoY);
    lv_obj_set_width(info_, kWidth - 16);
    lv_label_set_long_mode(info_, LV_LABEL_LONG_MODE_WRAP);
    ports_ = list_ports();
    refresh();
}

void PortPage::on_show() {
    ports_ = list_ports();
    refresh();
}

void PortPage::tick() {
    if (ctx_.engine.is_open() != was_open_) refresh();
}

std::string PortPage::value_of(int row) const {
    const auto& s = ctx_.session;
    const auto& eng = ctx_.engine;
    char buf[96];
    switch (row) {
        case RPort: {
            const auto info = describe_port(s.port);
            std::string v = short_port(s.port);
            if (!info.target.empty()) v += ">" + short_port(info.target);
            v += eng.is_open() ? " open" : " closed";
            return v;
        }
        case RBaud: return std::to_string(s.fmt.baud);
        case RFrame: return ser::frame_name(s.fmt);
        case RSplit: {
            if (s.split == ser::Split::Line) return "by line (LF)";
            if (s.gap_ms == 0) {
                std::snprintf(buf, sizeof buf, "gap auto (%.1f ms)", static_cast<double>(s.framer().gap_us) / 1000.0);
                return buf;
            }
            return "gap " + std::to_string(s.gap_ms) + " ms";
        }
        case RLabels: return s.modbus_labels ? "on" : "off";
        case RPoll: return std::to_string(s.poll_ms) + " ms";
        case RTimeout: return std::to_string(s.timeout_ms) + " ms";
        case RScan: return "#" + std::to_string(s.scan_first) + "..#" + std::to_string(s.scan_last);
        case RLog: return eng.logging() ? "on  " + short_port(eng.log_path()) : "off";
        default: return "";
    }
}

void PortPage::refresh() {
    if (!info_) return;
    const auto& st = ctx_.style;
    was_open_ = ctx_.engine.is_open();
    for (int i = 0; i < kRows; ++i) {
        const bool sel = i == sel_;
        lv_obj_set_style_bg_opa(rows_[i], sel ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(names_[i], sel ? st.text : st.dim, 0);
        lv_label_set_text(values_[i], value_of(i).c_str());
        lv_color_t c = sel ? st.accent : st.text;
        if (i == RPort) c = ctx_.engine.is_open() ? st.accent : st.error;
        lv_obj_set_style_text_color(values_[i], c, 0);
    }

    // the line under the list: what is wrong, or what the row does
    const auto& s = ctx_.session;
    const auto info = describe_port(s.port);
    std::string text;
    lv_color_t color = st.dim;
    if (!ctx_.engine.is_open() && !ctx_.engine.error().empty()) {
        text = ctx_.engine.error();
        color = st.error;
    } else if (sel_ == RPort && info.console) {
        text = "kernel console / login shell may be on this port";
        color = st.warn;
    } else if (sel_ == RPort && !info.accessible) {
        text = "no access: add the user to the dialout group";
        color = st.warn;
    } else if (sel_ == RPort) {
        text = "Fn+Z/C choose   Enter open/close   SIM = demo bus";
    } else if (sel_ == RSplit) {
        text = "gap: a pause ends a frame (Modbus)  line: LF ends it";
    } else if (sel_ == RLog) {
        text = "saved to ~/.config/cz-serialputer/logs/";
    } else {
        char buf[96];
        std::snprintf(buf, sizeof buf, "1 char = %.2f ms   Modbus gap %.2f ms",
                      static_cast<double>(ser::char_time_us(s.fmt)) / 1000.0,
                      static_cast<double>(ser::modbus_gap_us(s.fmt)) / 1000.0);
        text = buf;
    }
    lv_obj_set_style_text_color(info_, color, 0);
    lv_label_set_text(info_, text.c_str());
}

void PortPage::change(int dir) {
    auto& s = ctx_.session;
    auto& eng = ctx_.engine;
    switch (sel_) {
        case RPort: {
            if (ports_.empty()) return;
            int idx = 0;
            for (size_t i = 0; i < ports_.size(); ++i) {
                if (ports_[i].path == s.port) idx = static_cast<int>(i);
            }
            const int n = static_cast<int>(ports_.size());
            s.port = ports_[static_cast<size_t>((idx + dir + n) % n)].path;
            eng.close();
            break;
        }
        case RBaud:
            s.fmt.baud = ser::next_baud(s.fmt.baud, dir);
            eng.apply_format();
            break;
        case RFrame:
            ser::parse_frame(ser::next_frame(ser::frame_name(s.fmt), dir), s.fmt);
            eng.apply_format();
            break;
        case RSplit: {
            const int cur = s.split == ser::Split::Line ? -1 : static_cast<int>(s.gap_ms);
            const int next = cycle(kSplits, cur, dir);
            s.split = next < 0 ? ser::Split::Line : ser::Split::Gap;
            s.gap_ms = next < 0 ? 0 : static_cast<uint32_t>(next);
            eng.apply_format();
            break;
        }
        case RLabels: s.modbus_labels = !s.modbus_labels; break;
        case RPoll: s.poll_ms = cycle(kPolls, s.poll_ms, dir); break;
        case RTimeout: s.timeout_ms = cycle(kTimeouts, s.timeout_ms, dir); break;
        case RScan:
            s.scan_first = 1;
            s.scan_last = static_cast<uint8_t>(cycle(kScanLast, static_cast<int>(s.scan_last), dir));
            break;
        case RLog:
            if (!eng.set_logging(!eng.logging())) ctx_.toast(eng.error());
            break;
        default: break;
    }
    ctx_.mark_dirty();
    ctx_.status_changed();
    refresh();
}

void PortPage::activate() {
    auto& eng = ctx_.engine;
    if (sel_ != RPort) {
        change(1);
        return;
    }
    if (eng.is_open()) {
        eng.close();
        ctx_.toast("Port closed");
    } else if (eng.open()) {
        ctx_.toast("Opened " + ctx_.session.port);
    } else {
        ctx_.toast(eng.error());
    }
    ctx_.status_changed();
    refresh();
}

bool PortPage::on_key(const cz::KeyEvent& ev) {
    if (!ev.pressed_or_repeat()) return false;
    switch (ev.key) {
        case key::Up:
            sel_ = (sel_ + kRows - 1) % kRows;
            break;
        case key::Down:
            sel_ = (sel_ + 1) % kRows;
            break;
        case key::Left:
            change(-1);
            return true;
        case key::Right:
        case ' ':
            change(1);
            return true;
        case key::Enter:
            if (ev.phase == cz::KeyPhase::Press) activate();
            return true;
        default:
            return false;
    }
    refresh();
    return true;
}

}  // namespace ser_ui
