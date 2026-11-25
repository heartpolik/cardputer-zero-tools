#include "term_page.h"

#include <algorithm>

namespace ser_ui {
namespace {

namespace key = platform::key;

constexpr int kRowH = 14;
constexpr int kLogY = 1;
constexpr int kInputY = 130;
constexpr int kInputH = 22;
constexpr int kPadX = 4;
constexpr int kTimeChars = 7;

}  // namespace

void TermPage::build(lv_obj_t* parent) {
    const auto& s = ctx_.style;
    for (int i = 0; i < kRows; ++i) {
        const int y = kLogY + i * kRowH;
        times_[i] = make_label(parent, s.mono12, s.dim);
        dirs_[i] = make_label(parent, s.mono12, s.accent);
        texts_[i] = make_label(parent, s.mono12, s.text);
        for (auto* l : {times_[i], dirs_[i], texts_[i]}) {
            lv_obj_set_y(l, y);
            lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_CLIP);
        }
    }
    hold_label_ = make_label(parent, s.ui10, s.warn, "HOLD");
    lv_obj_set_style_bg_color(hold_label_, s.surface, 0);
    lv_obj_set_style_bg_opa(hold_label_, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(hold_label_, 3, 0);
    lv_obj_align(hold_label_, LV_ALIGN_TOP_RIGHT, -2, 1);
    lv_obj_add_flag(hold_label_, LV_OBJ_FLAG_HIDDEN);

    input_box_.build(parent, s, 4, kInputY, kWidth - 8, kInputH);
    seen_serial_ = ~0ull;
    refresh_log();
    refresh_input();
}

void TermPage::on_show() {
    seen_serial_ = ~0ull;
    refresh_log();
    refresh_input();
}

void TermPage::tick() {
    if (ctx_.engine.capture().serial() != seen_serial_) refresh_log();
}

std::vector<TermPage::Line> TermPage::lines_for(const ser::Frame& f, uint64_t prev_us, int cols) const {
    std::vector<Line> out;
    const auto& sess = ctx_.session;
    const auto wrapped = ser::wrap_frame(f, sess.view, cols);
    for (size_t i = 0; i < wrapped.size(); ++i) {
        Line l;
        if (i == 0) {
            l.time = ser::time_label(sess.time, f.t_us, ctx_.engine.capture().start_us(), prev_us);
            l.dir = f.dir == ser::Dir::Rx ? '<' : '>';
        }
        l.text = wrapped[i];
        out.push_back(std::move(l));
    }
    if (sess.modbus_labels && !f.note.empty()) {
        Line n;
        n.text = f.note;
        n.note = true;
        out.push_back(std::move(n));
    }
    return out;
}

void TermPage::refresh_log() {
    if (!texts_[0]) return;
    const auto& s = ctx_.style;
    const auto& cap = ctx_.engine.capture();
    seen_serial_ = cap.serial();

    const int cw = glyph_width(s.mono12);
    const bool show_time = ctx_.session.time != ser::TimeMode::Off;
    const int time_w = show_time ? (kTimeChars + 1) * cw : 0;
    const int dir_x = kPadX + time_w;
    const int text_x = dir_x + 2 * cw;
    const int cols = (kWidth - text_x - kPadX) / cw;

    // newest frames last; gather lines backwards until the window is covered
    const auto& frames = cap.frames();
    std::vector<Line> lines;
    const size_t want = static_cast<size_t>(kRows + scroll_);
    for (size_t k = frames.size(); k-- > 0 && lines.size() < want;) {
        const uint64_t prev = k > 0 ? frames[k - 1].t_us : frames[k].t_us;
        auto fl = lines_for(frames[k], prev, cols);
        lines.insert(lines.begin(), fl.begin(), fl.end());
    }
    // clamp scrolling to what exists
    const int total = static_cast<int>(lines.size());
    scroll_ = std::clamp(scroll_, 0, std::max(0, total - kRows));
    const int end = total - scroll_;
    const int begin = std::max(0, end - kRows);

    for (int i = 0; i < kRows; ++i) {
        const int idx = begin + i;
        if (idx >= end) {
            lv_label_set_text(times_[i], "");
            lv_label_set_text(dirs_[i], "");
            lv_label_set_text(texts_[i], "");
            continue;
        }
        const Line& l = lines[static_cast<size_t>(idx)];
        lv_obj_set_x(times_[i], kPadX);
        lv_obj_set_x(dirs_[i], dir_x);
        lv_obj_set_x(texts_[i], l.note ? text_x + cw : text_x);
        lv_label_set_text(times_[i], l.time.c_str());
        const char dir[2] = {l.dir, 0};
        lv_label_set_text(dirs_[i], dir);
        lv_obj_set_style_text_color(dirs_[i], l.dir == '>' ? s.warn : s.accent, 0);
        if (l.note) lv_obj_set_style_text_color(texts_[i], s.dim, 0);
        lv_label_set_text(texts_[i], l.text.c_str());
    }
    // sent frames in the warn colour, continuation lines take their frame's colour
    char current = ' ';
    for (int i = 0; i < kRows; ++i) {
        const int idx = begin + i;
        if (idx >= end) break;
        const Line& l = lines[static_cast<size_t>(idx)];
        if (l.dir != ' ') current = l.dir;
        if (!l.note) lv_obj_set_style_text_color(texts_[i], current == '>' ? s.warn : s.text, 0);
    }

    if (frames.empty()) {
        lv_obj_set_style_text_color(texts_[0], s.dim, 0);
        lv_label_set_text(texts_[0], ctx_.engine.is_open() ? "waiting for data..." : "port closed: Tab to PORT");
    }
    if (hold_) lv_obj_clear_flag(hold_label_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(hold_label_, LV_OBJ_FLAG_HIDDEN);
}

void TermPage::refresh_input() {
    if (!input_box_.box) return;
    const auto& s = ctx_.style;
    const auto& sess = ctx_.session;
    std::string tag, hint;
    if (sess.send == ser::SendMode::Hex) {
        tag = sess.append_crc ? "HEX+CRC" : "HEX";
        hint = sess.append_crc ? "01 03 00 00 00 0A" : "01 03 00 00 00 0A C5 CD";
    } else {
        tag = std::string("TXT+") + ser::line_end_name(sess.line_end);
        if (sess.line_end == ser::LineEnd::None) tag = "TXT";
        hint = "AT   (\\r \\n \\xHH)";
    }
    if (!sess.history.empty()) hint = "Enter: " + sess.history.back();
    lv_label_set_text(input_box_.tag, tag.c_str());
    lv_obj_update_layout(input_box_.tag);
    input_box_.show(s, input_.text(), input_.cursor(), hint, lv_obj_get_width(input_box_.tag) + 6);
}

void TermPage::send() {
    auto& sess = ctx_.session;
    std::string text = input_.text();
    if (text.empty()) {
        if (sess.history.empty()) return;
        text = sess.history.back();  // Enter on an empty line repeats the last one
    }
    const auto r = ser::prepare_send(text, sess.send, sess.line_end, sess.append_crc);
    if (!r.ok) {
        ctx_.toast(r.error);
        return;
    }
    if (!ctx_.engine.send(r.bytes)) {
        ctx_.toast(ctx_.engine.error());
        return;
    }
    sess.remember(text);
    ctx_.mark_dirty();
    input_.clear();
    hist_pos_ = -1;
    hold_ = false;
    scroll_ = 0;
    refresh_input();
    refresh_log();
}

void TermPage::history_step(int dir) {
    const auto& h = ctx_.session.history;
    if (h.empty()) return;
    const int n = static_cast<int>(h.size());
    if (hist_pos_ < 0) {
        if (dir > 0) return;
        draft_ = input_.text();
        hist_pos_ = n - 1;
    } else {
        hist_pos_ += dir;
        if (hist_pos_ >= n) {
            hist_pos_ = -1;
            input_.set(draft_);
            refresh_input();
            return;
        }
        hist_pos_ = std::max(hist_pos_, 0);
    }
    input_.set(h[static_cast<size_t>(hist_pos_)]);
    refresh_input();
}

bool TermPage::on_key(const cz::KeyEvent& ev) {
    if (!ev.pressed_or_repeat()) return false;
    const bool press = ev.phase == cz::KeyPhase::Press;
    auto& sess = ctx_.session;
    auto& eng = ctx_.engine;

    if (press) {
        switch (ev.key) {
            case key::F(2):
                sess.view = ser::next_view(sess.view);
                ctx_.toast(std::string("View ") + ser::view_name(sess.view));
                ctx_.mark_dirty();
                refresh_log();
                return true;
            case key::F(3):
                sess.send = sess.send == ser::SendMode::Text ? ser::SendMode::Hex : ser::SendMode::Text;
                ctx_.mark_dirty();
                refresh_input();
                return true;
            case key::F(4):
                if (sess.send == ser::SendMode::Hex) sess.append_crc = !sess.append_crc;
                else sess.line_end = ser::next_line_end(sess.line_end);
                ctx_.mark_dirty();
                refresh_input();
                return true;
            case key::F(5):
                sess.time = ser::next_time_mode(sess.time);
                ctx_.toast(std::string("Time ") + ser::time_mode_name(sess.time));
                ctx_.mark_dirty();
                refresh_log();
                return true;
            case key::F(7):
                hold_ = !hold_;
                scroll_ = 0;
                refresh_log();
                return true;
            case key::F(8):
                eng.clear_capture();
                scroll_ = 0;
                refresh_log();
                ctx_.status_changed();
                return true;
            case key::F(9):
                if (eng.set_logging(!eng.logging())) {
                    ctx_.toast(eng.logging() ? "Log: " + eng.log_path() : "Log closed");
                } else {
                    ctx_.toast(eng.error());
                }
                ctx_.status_changed();
                return true;
            case key::F(10):
                sess.modbus_labels = !sess.modbus_labels;
                ctx_.toast(sess.modbus_labels ? "Modbus labels on" : "Modbus labels off");
                ctx_.mark_dirty();
                refresh_log();
                return true;
            default:
                break;
        }
    }

    switch (ev.key) {
        case key::Enter:
            if (press) send();
            return true;
        case key::Up:
            if (hold_) {
                ++scroll_;
                refresh_log();
            } else {
                history_step(-1);
            }
            return true;
        case key::Down:
            if (hold_) {
                if (scroll_ > 0) --scroll_;
                refresh_log();
            } else {
                history_step(1);
            }
            return true;
        case key::Esc:
            if (!press) return true;
            if (hold_) {
                hold_ = false;
                scroll_ = 0;
                refresh_log();
                return true;
            }
            if (input_.empty()) return false;
            input_.clear();
            break;
        case key::Backspace: input_.backspace(); break;
        case key::Delete: input_.del(); break;
        case key::Left: input_.left(); break;
        case key::Right: input_.right(); break;
        case key::Home: input_.home(); break;
        case key::End: input_.end(); break;
        default:
            if (!key::is_printable(ev.key)) return false;
            input_.insert(static_cast<char>(ev.key));
            break;
    }
    hist_pos_ = -1;
    refresh_input();
    return true;
}

}  // namespace ser_ui
