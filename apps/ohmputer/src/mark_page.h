#pragma once

#include "app_context.h"
#include "line_editor.h"

namespace ohm_ui {

// Printed codes -> every plausible reading (resistor / capacitor / inductor / part).
class MarkPage : public Page {
public:
    static constexpr int kRows = 4;

    explicit MarkPage(AppContext& ctx) : ctx_(ctx) {}

    void build(lv_obj_t* parent) override;
    void on_show() override { refresh(); }
    bool on_key(const cz::KeyEvent& ev) override;
    void on_series_changed() override { refresh(); }

private:
    void refresh();

    AppContext& ctx_;
    cz::LineEditor input_;
    lv_obj_t* parent_{nullptr};
    lv_obj_t* input_box_{nullptr};
    lv_obj_t* input_label_{nullptr};
    lv_obj_t* cursor_{nullptr};
    lv_obj_t* results_{nullptr};
};

}  // namespace ohm_ui
