#pragma once

#include "app_context.h"

namespace ohm_ui {

// Formula tools (Ohm, divider, LED, RC, LC, battery): type any inputs, the rest is computed.
class ToolsPage : public Page {
public:
    explicit ToolsPage(AppContext& ctx) : ctx_(ctx) {}

    void build(lv_obj_t* parent) override;
    void on_show() override { refresh(); }
    bool on_key(const cz::KeyEvent& ev) override;
    void on_series_changed() override { refresh(); }
    const char* status() const override;

private:
    ohm::Form& form();
    void select(int dir);
    void refresh();

    AppContext& ctx_;
    int selected_{0};
    lv_obj_t* area_{nullptr};
};

}  // namespace ohm_ui
