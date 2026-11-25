#include "modbus_page.h"

#include <algorithm>
#include <cstdio>

namespace ser_ui {
namespace {

namespace key = platform::key;
namespace mb = ser::modbus;

constexpr int kFieldY = 2;
constexpr int kFieldH = 18;
constexpr int kStatusY = 23;
constexpr int kHeaderY = 38;
constexpr int kRowsY = 52;
constexpr int kRowH = 14;
constexpr int kFieldX[4] = {4, 58, 168, 250};
constexpr int kFieldW[4] = {50, 106, 78, 66};

const char* fn_short(uint8_t fc) {
    switch (fc) {
        case mb::kReadCoils: return "COILS";
        case mb::kReadDiscrete: return "INPUTS";
        case mb::kReadHolding: return "HOLDING";
        case mb::kReadInput: return "INPUT";
        default: return "?";
    }
}

}  // namespace

void ModbusPage::build(lv_obj_t* parent) {
    const auto& s = ctx_.style;
    for (int i = 0; i < kFields; ++i) {
        fields_[i] = make_box(parent, kFieldX[i], kFieldY, kFieldW[i], kFieldH);
        lv_obj_set_style_radius(fields_[i], 3, 0);
        lv_obj_set_style_bg_color(fields_[i], s.surface, 0);
        lv_obj_set_style_bg_opa(fields_[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(fields_[i], s.accent, 0);
        field_text_[i] = make_label(fields_[i], s.mono12, s.text);
        lv_obj_align(field_text_[i], LV_ALIGN_LEFT_MID, 4, 0);
    }
    status_ = make_label(parent, s.ui10, s.dim);
    lv_obj_set_pos(status_, 6, kStatusY);
    lv_obj_set_width(status_, 250);
    lv_label_set_long_mode(status_, LV_LABEL_LONG_MODE_DOTS);
    interp_ = make_label(parent, s.ui10, s.warn);
    lv_obj_align(interp_, LV_ALIGN_TOP_RIGHT, -6, kStatusY);

    header_ = make_label(parent, s.mono12, s.dim);
    lv_obj_set_pos(header_, 6, kHeaderY);
    for (int i = 0; i < kRows; ++i) {
        rows_[i] = make_label(parent, s.mono12, s.text);
        lv_obj_set_pos(rows_[i], 6, kRowsY + i * kRowH);
        lv_label_set_long_mode(rows_[i], LV_LABEL_LONG_MODE_CLIP);
        lv_obj_set_width(rows_[i], kWidth - 12);
    }
    refresh();
}

void ModbusPage::on_show() {
    fresh_ = true;
    refresh();
}

void ModbusPage::tick() {
    if (ctx_.engine.modbus_serial() != seen_) refresh();
}

void ModbusPage::refresh() {
    if (!status_) return;
    seen_ = ctx_.engine.modbus_serial();
    refresh_fields();
    refresh_status();
    if (show_scan_) refresh_scan();
    else refresh_table();
}

uint32_t ModbusPage::field_value(int f) const {
    const auto& r = ctx_.session.req;
    switch (f) {
        case Addr: return r.addr;
        case Fn: return r.fc;
        case Reg: return r.start;
        default: return r.count;
    }
}

void ModbusPage::set_field_value(int f, uint32_t v) {
    auto& r = ctx_.session.req;
    switch (f) {
        case Addr: r.addr = static_cast<uint8_t>(std::min<uint32_t>(v, 247)); break;
        case Fn:
            if (mb::is_read(static_cast<uint8_t>(v))) r.fc = static_cast<uint8_t>(v);
            r.count = std::min<uint16_t>(r.count, mb::max_count(r.fc));
            break;
        case Reg: r.start = static_cast<uint16_t>(std::min<uint32_t>(v, 65535)); break;
        default: r.count = static_cast<uint16_t>(std::clamp<uint32_t>(v, 1, mb::max_count(r.fc))); break;
    }
    ctx_.mark_dirty();
}

void ModbusPage::refresh_fields() {
    const auto& s = ctx_.style;
    const auto& r = ctx_.session.req;
    char buf[4][32];
    std::snprintf(buf[0], sizeof buf[0], "ID %u", r.addr);
    std::snprintf(buf[1], sizeof buf[1], "FN %02u %s", r.fc, fn_short(r.fc));
    std::snprintf(buf[2], sizeof buf[2], "REG %u", r.start);
    std::snprintf(buf[3], sizeof buf[3], "N %u", r.count);
    for (int i = 0; i < kFields; ++i) {
        const bool sel = i == field_;
        lv_obj_set_style_border_width(fields_[i], sel ? 1 : 0, 0);
        lv_obj_set_style_bg_color(fields_[i], sel ? s.selection : s.surface, 0);
        lv_label_set_text(field_text_[i], buf[i]);
    }
}

void ModbusPage::refresh_status() {
    const auto& s = ctx_.style;
    const auto& eng = ctx_.engine;
    const auto& st = eng.master().stats();
    lv_label_set_text(interp_, mb::is_bit_fn(ctx_.session.req.fc) ? "BITS" : mb::interp_name(ctx_.session.interp));

    char buf[120];
    lv_color_t color = s.dim;
    if (!eng.is_open()) {
        std::snprintf(buf, sizeof buf, "port closed%s%s", eng.error().empty() ? "" : ": ", eng.error().c_str());
        color = s.error;
    } else if (eng.scanning()) {
        const auto& sc = eng.scanner();
        std::snprintf(buf, sizeof buf, "SCAN #%u of %u..%u  found %zu  (Esc stops)", sc.current(), sc.first(),
                      sc.last(), sc.found().size());
        color = s.warn;
    } else if (st.sent == 0) {
        std::snprintf(buf, sizeof buf, "Enter read   F3 poll   F5 scan");
    } else {
        const auto& last = eng.master().last();
        const char* mode = eng.polling() ? "POLL " : "";
        if (eng.master().busy()) {
            std::snprintf(buf, sizeof buf, "%swaiting...  ok %u/%u", mode, st.ok, st.sent);
        } else if (last.ok) {
            std::snprintf(buf, sizeof buf, "%sOK %u ms   ok %u/%u  t/o %u", mode, st.last_ms, st.ok, st.sent,
                          st.timeouts);
            color = eng.polling() ? s.accent : s.dim;
        } else {
            std::snprintf(buf, sizeof buf, "%s%s   ok %u/%u  t/o %u", mode, last.error.c_str(), st.ok, st.sent,
                          st.timeouts);
            color = s.error;
        }
    }
    lv_obj_set_style_text_color(status_, color, 0);
    lv_label_set_text(status_, buf);
}

void ModbusPage::refresh_table() {
    const auto& s = ctx_.style;
    const auto& eng = ctx_.engine;
    const auto interp = ctx_.session.interp;
    const auto& req = eng.table_request();
    const auto& t = eng.table();

    for (auto* r : rows_) lv_label_set_text(r, "");
    if (!eng.has_table()) {
        lv_label_set_text(header_, "");
        lv_obj_set_style_text_color(rows_[1], s.dim, 0);
        lv_label_set_text(rows_[1], "no data yet");
        return;
    }
    char line[96];
    if (mb::is_bit_fn(req.fc)) {
        lv_label_set_text(header_, "BIT   0 1 2 3 4 5 6 7");
        const int n_rows = static_cast<int>((t.bits.size() + 7) / 8);
        first_ = std::clamp(first_, 0, std::max(0, n_rows - kRows));
        for (int i = 0; i < kRows; ++i) {
            const int row = first_ + i;
            if (row >= n_rows) break;
            int p = std::snprintf(line, sizeof line, "%-5u", req.start + row * 8);
            for (int b = 0; b < 8; ++b) {
                const size_t idx = static_cast<size_t>(row * 8 + b);
                if (idx >= t.bits.size()) break;
                p += std::snprintf(line + p, sizeof line - static_cast<size_t>(p), " %c", t.bits[idx] ? '1' : '0');
            }
            lv_obj_set_style_text_color(rows_[i], s.text, 0);
            lv_label_set_text(rows_[i], line);
        }
        return;
    }

    const bool pair = mb::is_pair(interp);
    const int step = pair ? 2 : 1;
    std::snprintf(line, sizeof line, "%-6s%-*s%s", "REG", pair ? 13 : 8, "RAW", mb::interp_name(interp));
    lv_label_set_text(header_, line);
    const int n = static_cast<int>(t.regs.size());
    const int n_rows = (n + step - 1) / step;
    first_ = std::clamp(first_, 0, std::max(0, n_rows - kRows));
    for (int i = 0; i < kRows; ++i) {
        const int row = first_ + i;
        if (row >= n_rows) break;
        const size_t idx = static_cast<size_t>(row * step);
        const std::string value = mb::format_value(t.regs, idx, interp);
        if (pair) {
            if (idx + 1 < t.regs.size()) {
                std::snprintf(line, sizeof line, "%-6u%04X %04X    %s", static_cast<unsigned>(req.start + idx),
                              t.regs[idx], t.regs[idx + 1], value.c_str());
            } else {
                std::snprintf(line, sizeof line, "%-6u%04X", static_cast<unsigned>(req.start + idx), t.regs[idx]);
            }
        } else {
            std::snprintf(line, sizeof line, "%-6u%04X    %s", static_cast<unsigned>(req.start + idx), t.regs[idx],
                          value.c_str());
        }
        lv_obj_set_style_text_color(rows_[i], s.text, 0);
        lv_label_set_text(rows_[i], line);
    }
}

void ModbusPage::refresh_scan() {
    const auto& s = ctx_.style;
    const auto& sc = ctx_.engine.scanner();
    char line[96];
    std::snprintf(line, sizeof line, "SCAN FN %02u REG %u   %s", ctx_.session.req.fc, ctx_.session.req.start,
                  sc.running() ? "running" : "done");
    lv_label_set_text(header_, line);
    for (auto* r : rows_) lv_label_set_text(r, "");
    const auto& found = sc.found();
    if (found.empty()) {
        lv_obj_set_style_text_color(rows_[1], s.dim, 0);
        lv_label_set_text(rows_[1], sc.running() ? "listening..." : "no device answered");
        return;
    }
    first_ = std::clamp(first_, 0, std::max(0, static_cast<int>(found.size()) - kRows));
    for (int i = 0; i < kRows; ++i) {
        const size_t idx = static_cast<size_t>(first_ + i);
        if (idx >= found.size()) break;
        const auto& f = found[idx];
        if (f.exception) {
            std::snprintf(line, sizeof line, "#%-4u answers: %s", f.addr, mb::exception_name(f.code));
        } else {
            std::snprintf(line, sizeof line, "#%-4u answers", f.addr);
        }
        lv_obj_set_style_text_color(rows_[i], f.exception ? s.warn : s.accent, 0);
        lv_label_set_text(rows_[i], line);
    }
}

void ModbusPage::type_digit(char c) {
    if (field_ == Fn) {
        set_field_value(Fn, static_cast<uint32_t>(c - '0'));
        return;
    }
    const uint64_t cur = fresh_ ? 0 : field_value(field_);
    const uint64_t next = cur * 10 + static_cast<uint64_t>(c - '0');
    set_field_value(field_, static_cast<uint32_t>(std::min<uint64_t>(next, 65535)));
    fresh_ = false;
}

void ModbusPage::erase_digit() {
    if (field_ == Fn) return;
    set_field_value(field_, field_value(field_) / 10);
    fresh_ = false;
}

bool ModbusPage::on_key(const cz::KeyEvent& ev) {
    if (!ev.pressed_or_repeat()) return false;
    const bool press = ev.phase == cz::KeyPhase::Press;
    auto& eng = ctx_.engine;
    auto& sess = ctx_.session;

    switch (ev.key) {
        case key::Enter:
            if (!press) return true;
            show_scan_ = false;
            eng.read_once();
            if (!eng.is_open()) ctx_.toast("Port is closed: Tab to PORT");
            break;
        case key::F(2):
            if (!press) return true;
            sess.interp = mb::next_interp(sess.interp);
            ctx_.mark_dirty();
            break;
        case key::F(3):
            if (!press) return true;
            show_scan_ = false;
            eng.set_polling(!eng.polling());
            if (!eng.is_open()) ctx_.toast("Port is closed: Tab to PORT");
            ctx_.status_changed();
            break;
        case key::F(5):
            if (!press) return true;
            if (eng.scanning()) {
                eng.stop_scan();
            } else {
                show_scan_ = true;
                first_ = 0;
                eng.start_scan();
            }
            ctx_.status_changed();
            break;
        case key::F(8):
            if (!press) return true;
            eng.reset_stats();
            break;
        case key::Esc:
            if (!press) return true;
            if (eng.scanning()) eng.stop_scan();
            else if (eng.polling()) eng.set_polling(false);
            else if (show_scan_) show_scan_ = false;
            else return false;
            ctx_.status_changed();
            break;
        case key::Left:
            field_ = (field_ + kFields - 1) % kFields;
            fresh_ = true;
            break;
        case key::Right:
        case ' ':
            if (ev.key == ' ' && field_ == Fn) {
                set_field_value(Fn, mb::next_read_fn(sess.req.fc, 1));
                break;
            }
            field_ = (field_ + 1) % kFields;
            fresh_ = true;
            break;
        case key::Up:
            first_ = std::max(0, first_ - 1);
            break;
        case key::Down:
            ++first_;
            break;
        case key::Backspace:
            erase_digit();
            break;
        default:
            if (ev.key >= '0' && ev.key <= '9') {
                type_digit(static_cast<char>(ev.key));
                break;
            }
            return false;
    }
    refresh();
    return true;
}

}  // namespace ser_ui
