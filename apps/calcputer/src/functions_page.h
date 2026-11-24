#pragma once

#include "app_context.h"
#include "completion.h"
#include "line_editor.h"

#include <array>

namespace calc_ui {

// "Y=" editor: four plot slots.
class FunctionsPage : public Page {
public:
    explicit FunctionsPage(AppContext& ctx) : ctx_(ctx) {}

    void build(lv_obj_t* parent) override;
    void on_show() override;
    bool on_key(const cz::KeyEvent& ev) override;
    void on_engine_changed() override;

private:
    struct Row {
        lv_obj_t* box{nullptr};
        lv_obj_t* name{nullptr};
        lv_obj_t* text{nullptr};
        lv_obj_t* cursor{nullptr};
        lv_obj_t* error{nullptr};
    };

    void load_from_engine();
    void commit_row(size_t i);
    void refresh_row(size_t i);
    void refresh_all();

    AppContext& ctx_;
    std::array<calc::LineEditor, calc::Engine::kPlotSlots> edit_{};
    std::array<Row, calc::Engine::kPlotSlots> rows_{};
    size_t current_{0};
    calc::Completer completer_;
};

}  // namespace calc_ui
