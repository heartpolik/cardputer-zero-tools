#pragma once

#include "app_context.h"
#include "calc_page.h"
#include "crc_page.h"

#include <memory>

namespace bit_ui {

class BitApp {
public:
    explicit BitApp(cz::Runtime& rt);
    ~BitApp();

    bool on_key(const cz::KeyEvent& ev);
    void save();

private:
    void build();
    void destroy();
    void show(PageId id);
    void refresh_status();
    void modes_changed();
    void toggle_help();
    void load();
    Page& page(PageId id);

    cz::Runtime& rt_;
    bits::Session session_;
    Style style_;
    AppContext ctx_;
    PageId current_{PageId::Calc};
    bool dirty_{false};
    lv_timer_t* save_timer_{nullptr};

    std::unique_ptr<CalcPage> calc_;
    std::unique_ptr<CrcPage> crc_;

    lv_obj_t* root_{nullptr};
    lv_obj_t* tabs_[2]{};
    lv_obj_t* mode_label_{nullptr};
    lv_obj_t* pages_[2]{};
    lv_obj_t* help_{nullptr};
    lv_obj_t* help_text_{nullptr};
};

}  // namespace bit_ui
