#pragma once

#include "app_context.h"
#include "calc_page.h"
#include "functions_page.h"
#include "graph_page.h"

#include <memory>

namespace calc_ui {

class CalcApp {
public:
    explicit CalcApp(cz::Runtime& rt);
    ~CalcApp();

    bool on_key(const cz::KeyEvent& ev);
    void save();

private:
    void build();
    void destroy();
    void show(PageId id);
    void refresh_status();
    void toggle_help();
    void scroll_help(int dir);
    void load();
    Page& page(PageId id);

    cz::Runtime& rt_;
    calc::Engine engine_;
    Style style_;
    AppContext ctx_;
    PageId current_{PageId::Calc};
    bool dirty_{false};
    lv_timer_t* save_timer_{nullptr};

    std::unique_ptr<CalcPage> calc_;
    std::unique_ptr<FunctionsPage> functions_;
    std::unique_ptr<GraphPage> graph_;

    lv_obj_t* root_{nullptr};
    lv_obj_t* tabs_[3]{};
    lv_obj_t* mode_label_{nullptr};
    lv_obj_t* pages_[3]{};
    lv_obj_t* help_{nullptr};
    lv_obj_t* help_text_{nullptr};
};

}  // namespace calc_ui
