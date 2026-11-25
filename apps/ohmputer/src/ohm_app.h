#pragma once

#include "app_context.h"
#include "bands_page.h"
#include "mark_page.h"
#include "tools_page.h"

#include "shortcut_bar.h"

#include <memory>

namespace ohm_ui {

class OhmApp {
public:
    explicit OhmApp(cz::Runtime& rt);
    ~OhmApp();

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
    ohm::Session session_;
    Style style_;
    AppContext ctx_;
    PageId current_{PageId::Mark};
    bool dirty_{false};
    lv_timer_t* save_timer_{nullptr};

    std::unique_ptr<MarkPage> mark_;
    std::unique_ptr<BandsPage> bands_;
    std::unique_ptr<ToolsPage> tools_;

    lv_obj_t* root_{nullptr};
    lv_obj_t* tabs_[kPages]{};
    lv_obj_t* mode_label_{nullptr};
    lv_obj_t* pages_[kPages]{};
    lv_obj_t* help_{nullptr};
    cz::ShortcutBar layer_bar_;  // keys 4..8 while Fn / Sym / Shift is active
    lv_obj_t* help_text_{nullptr};
};

}  // namespace ohm_ui
