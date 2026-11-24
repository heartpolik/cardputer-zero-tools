#pragma once

#include "app_context.h"
#include "line_editor.h"

#include <string>

namespace bit_ui {

// Expression input, the value in every base, another reading (chars/float/time)
// and an editable bit grid.
class CalcPage : public Page {
public:
    explicit CalcPage(AppContext& ctx) : ctx_(ctx) {}

    void build(lv_obj_t* parent) override;
    void on_show() override;
    bool on_key(const cz::KeyEvent& ev) override;
    void on_mode_changed() override;

    void set_input(const std::string& text);

private:
    bool on_grid_key(const cz::KeyEvent& ev);
    bool on_orders_key(const cz::KeyEvent& ev);
    int order_rows() const;  // byte orders worth showing for the word width
    void refresh_orders();
    void submit();
    void history_step(int dir);
    void edit_value(uint64_t value);  // from the grid: input becomes this literal
    uint64_t shown_value() const;
    void refresh();
    void refresh_input();
    void refresh_values();
    void refresh_grid();
    void refresh_message();

    AppContext& ctx_;
    cz::LineEditor input_;
    std::string draft_;   // input before browsing history
    int hist_pos_{-1};    // index into session.history while browsing
    bool grid_{false};    // bit grid has focus
    int bit_{0};          // grid cursor (bit index)
    std::string message_;
    bool live_ok_{false};
    uint64_t live_value_{0};
    std::string live_error_;
    size_t live_error_pos_{std::string::npos};
    std::string live_assign_;

    lv_obj_t* input_box_{nullptr};
    lv_obj_t* input_label_{nullptr};
    lv_obj_t* cursor_{nullptr};
    lv_obj_t* message_label_{nullptr};
    lv_obj_t* names_[4]{};
    lv_obj_t* values_[4]{};
    lv_obj_t* extras_[4]{};
    lv_obj_t* grid_box_{nullptr};
    lv_obj_t* grid_cursor_{nullptr};
    lv_obj_t* grid_rows_[2]{};
    lv_obj_t* grid_index_[2][8]{};
    lv_obj_t* order_cursor_{nullptr};
    lv_obj_t* order_rows_[4]{};
    int order_sel_{0};
};

}  // namespace bit_ui
