#pragma once

#include "app_context.h"
#include "checksum.h"
#include "line_editor.h"

namespace bit_ui {

// Bytes (hex or "text") -> every common CRC / checksum at once.
class CrcPage : public Page {
public:
    static constexpr int kVisibleRows = 8;

    explicit CrcPage(AppContext& ctx) : ctx_(ctx) {}

    void build(lv_obj_t* parent) override;
    void on_show() override;
    bool on_key(const cz::KeyEvent& ev) override;

private:
    void refresh();
    void refresh_input();
    void refresh_rows();

    AppContext& ctx_;
    cz::LineEditor input_;
    bits::ByteParse parsed_;
    int first_{0};

    lv_obj_t* input_box_{nullptr};
    lv_obj_t* input_label_{nullptr};
    lv_obj_t* cursor_{nullptr};
    lv_obj_t* message_{nullptr};
    lv_obj_t* rows_[kVisibleRows]{};
    lv_obj_t* row_text_[kVisibleRows]{};
};

}  // namespace bit_ui
