#pragma once

#include "app_context.h"

#include <vector>

namespace ser_ui {

// Port, line settings, framing, Modbus timing and the log file.
class PortPage : public Page {
public:
    static constexpr int kRows = 9;

    explicit PortPage(AppContext& ctx) : ctx_(ctx) {}

    void build(lv_obj_t* parent) override;
    void on_show() override;
    bool on_key(const cz::KeyEvent& ev) override;
    void tick() override;

private:
    enum Row { RPort, RBaud, RFrame, RSplit, RLabels, RPoll, RTimeout, RScan, RLog };

    void refresh();
    void change(int dir);
    void activate();
    std::string value_of(int row) const;

    AppContext& ctx_;
    int sel_{0};
    std::vector<PortInfo> ports_;
    bool was_open_{false};

    lv_obj_t* rows_[kRows]{};
    lv_obj_t* names_[kRows]{};
    lv_obj_t* values_[kRows]{};
    lv_obj_t* info_{nullptr};
};

}  // namespace ser_ui
