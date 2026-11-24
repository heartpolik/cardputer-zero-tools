#pragma once

#include "engine.h"
#include "runtime.h"
#include "style.h"

#include <functional>

namespace calc_ui {

enum class PageId { Calc = 0, Functions = 1, Graph = 2 };

// What pages share: services + callbacks into the app shell.
struct AppContext {
    cz::Runtime& rt;
    calc::Engine& engine;
    const Style& style;
    std::function<void()> mark_dirty;      // engine state changed -> persist soon
    std::function<void(PageId)> go;        // switch page
    std::function<void()> status_changed;  // angle/format changed -> refresh status bar
};

class Page {
public:
    virtual ~Page() = default;
    // parent: content area below the status bar (kWidth x kHeight - kStatusHeight)
    virtual void build(lv_obj_t* parent) = 0;
    virtual void on_show() {}
    virtual bool on_key(const cz::KeyEvent& ev) = 0;
    virtual void on_engine_changed() {}
};

}  // namespace calc_ui
