#pragma once

#include "app_context.h"
#include "line_editor.h"

namespace ohm_ui {

// Colour bands -> value, or (Space) a value -> its 4- and 5-band codes.
class BandsPage : public Page {
public:
    explicit BandsPage(AppContext& ctx) : ctx_(ctx) {}

    void build(lv_obj_t* parent) override;
    void on_show() override { refresh(); }
    bool on_key(const cz::KeyEvent& ev) override;
    void on_series_changed() override { refresh(); }
    const char* status() const override;

private:
    void refresh();
    void refresh_colors_mode();
    void refresh_value_mode();

    AppContext& ctx_;
    cz::LineEditor value_;
    lv_obj_t* area_{nullptr};
};

}  // namespace ohm_ui
