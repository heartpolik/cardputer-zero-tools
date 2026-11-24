#pragma once

#include "app_context.h"
#include "line_editor.h"

#include <string>

namespace calc_ui {

// Main calculator: history list, live preview and the input line.
class CalcPage : public Page {
public:
    explicit CalcPage(AppContext& ctx) : ctx_(ctx) {}

    void build(lv_obj_t* parent) override;
    void on_show() override;
    bool on_key(const cz::KeyEvent& ev) override;
    void on_engine_changed() override;

    // inserts text at the cursor (used by other pages / helpers)
    void insert(const std::string& text);

private:
    void submit();
    void rebuild_history();
    void refresh_selection();
    void refresh_input();
    void refresh_preview();
    void refresh_footer();
    void set_message(const std::string& msg, bool error);

    AppContext& ctx_;
    calc::LineEditor input_;
    int selected_{-1};  // index into engine.history, -1 = editing
    std::string message_;
    bool message_error_{false};

    lv_obj_t* history_{nullptr};
    lv_obj_t* empty_hint_{nullptr};
    lv_obj_t* preview_{nullptr};
    lv_obj_t* input_box_{nullptr};
    lv_obj_t* input_label_{nullptr};
    lv_obj_t* cursor_{nullptr};
    lv_obj_t* footer_{nullptr};
    int first_rendered_{0};  // engine.history index of the first rendered entry
};

}  // namespace calc_ui
