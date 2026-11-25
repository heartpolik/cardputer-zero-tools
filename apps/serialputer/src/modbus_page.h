#pragma once

#include "app_context.h"

namespace ser_ui {

// Modbus RTU master: request form, register table, polling and address scan.
class ModbusPage : public Page {
public:
    static constexpr int kRows = 7;
    static constexpr int kFields = 4;

    explicit ModbusPage(AppContext& ctx) : ctx_(ctx) {}

    void build(lv_obj_t* parent) override;
    void on_show() override;
    bool on_key(const cz::KeyEvent& ev) override;
    void tick() override;

private:
    enum Field { Addr = 0, Fn = 1, Reg = 2, Count = 3 };

    void refresh();
    void refresh_fields();
    void refresh_status();
    void refresh_table();
    void refresh_scan();
    void type_digit(char c);
    void erase_digit();
    uint32_t field_value(int f) const;
    void set_field_value(int f, uint32_t v);

    AppContext& ctx_;
    int field_{Addr};
    bool fresh_{true};  // the next digit replaces the field
    int first_{0};      // first table row shown
    bool show_scan_{false};
    uint64_t seen_{~0ull};

    lv_obj_t* fields_[kFields]{};
    lv_obj_t* field_text_[kFields]{};
    lv_obj_t* status_{nullptr};
    lv_obj_t* interp_{nullptr};
    lv_obj_t* header_{nullptr};
    lv_obj_t* rows_[kRows]{};
};

}  // namespace ser_ui
