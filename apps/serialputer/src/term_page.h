#pragma once

#include "app_context.h"
#include "line_editor.h"

namespace ser_ui {

// The terminal: frames as they come (RX / TX), an input line to send text or bytes.
class TermPage : public Page {
public:
    static constexpr int kRows = 9;

    explicit TermPage(AppContext& ctx) : ctx_(ctx) {}

    void build(lv_obj_t* parent) override;
    void on_show() override;
    bool on_key(const cz::KeyEvent& ev) override;
    void tick() override;

private:
    struct Line {
        std::string time;
        char dir{' '};  // '<' received, '>' sent, ' ' continuation / note
        std::string text;
        bool note{false};
    };

    void send();
    void history_step(int dir);
    void refresh_log();
    void refresh_input();
    std::vector<Line> lines_for(const ser::Frame& f, uint64_t prev_us, int cols) const;

    AppContext& ctx_;
    cz::LineEditor input_;
    std::string draft_;
    int hist_pos_{-1};
    bool hold_{false};  // scrolled back: the view stops following new data
    int scroll_{0};     // lines above the newest
    uint64_t seen_serial_{~0ull};

    lv_obj_t* times_[kRows]{};
    lv_obj_t* dirs_[kRows]{};
    lv_obj_t* texts_[kRows]{};
    lv_obj_t* hold_label_{nullptr};
    InputBox input_box_;
};

}  // namespace ser_ui
