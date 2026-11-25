#pragma once

#include "app_context.h"
#include "modbus_page.h"
#include "port_page.h"
#include "term_page.h"

#include "shortcut_bar.h"

#include <memory>

namespace ser_ui {

class SerialApp {
public:
    explicit SerialApp(cz::Runtime& rt);
    ~SerialApp();

    bool on_key(const cz::KeyEvent& ev);
    void save();

private:
    void build();
    void destroy();
    void show(PageId id);
    void refresh_status();
    void toggle_help();
    void refresh_layer_bar();
    void load();
    Page& page(PageId id);

    cz::Runtime& rt_;
    ser::Session session_;
    Style style_;
    std::unique_ptr<Engine> engine_;
    AppContext ctx_;
    PageId current_{PageId::Term};
    bool dirty_{false};
    lv_timer_t* io_timer_{nullptr};
    lv_timer_t* ui_timer_{nullptr};
    lv_timer_t* save_timer_{nullptr};

    std::unique_ptr<TermPage> term_;
    std::unique_ptr<ModbusPage> modbus_;
    std::unique_ptr<PortPage> port_;

    lv_obj_t* root_{nullptr};
    lv_obj_t* tabs_[kPageCount]{};
    lv_obj_t* port_label_{nullptr};
    lv_obj_t* pages_[kPageCount]{};
    lv_obj_t* help_{nullptr};
    cz::ShortcutBar layer_bar_;  // keys 4..8 while Fn / Sym / Shift is active
    lv_obj_t* help_text_{nullptr};
};

}  // namespace ser_ui
