#include "calc_app.h"

#include "logger.h"

#include <cstdio>
#include <fstream>
#include <sstream>

namespace calc_ui {
namespace {

namespace key = platform::key;

constexpr const char* kTabNames[3] = {"CALC", "Y=", "GRAPH"};

const char* help_for(PageId id) {
    switch (id) {
        case PageId::Calc:
            return "#63e2b7 CALC#\n"
                   "Enter      evaluate (empty: repeat last)\n"
                   "Fn+F / X   browse history\n"
                   "  Enter    insert result   Space  expr\n"
                   "Esc        clear line (hold: quit)\n"
                   "*2  ^2     start with op -> uses ans\n"
                   "a = 5      variable    f(x)=x^2  function\n"
                   "\n"
                   "#63e2b7 FUNCTIONS#\n"
                   "sin cos tan asin acos atan atan2\n"
                   "sinh cosh tanh asinh acosh atanh\n"
                   "sqrt cbrt root(x,n) abs exp ln log\n"
                   "log(b,x) log2 log10 floor ceil round\n"
                   "trunc sign mod min max hypot gamma\n"
                   "gcd lcm ncr npr fact  n!  torad todeg\n"
                   "pi e tau phi ans   0x1F 0b101 1e-3\n"
                   "\n"
                   "#63e2b7 CALCULUS# (expression in x)\n"
                   "der(x^3, 2)        derivative at 2\n"
                   "integ(sin(x),0,pi) integral\n"
                   "solve(x^2-2, 1)    root near 1\n"
                   "solve(cos(x)-x,0,1) root in [0,1]\n"
                   "sum(1/x^2, 1, 100) sum over x\n"
                   "\n"
                   "#63e2b7 GLOBAL#\n"
                   "Tab / Shift+Tab   CALC > Y= > GRAPH\n"
                   "F2 deg/rad  F3 AUTO/SCI/ENG  F4 theme\n"
                   "F5 clear history  Fn+J screenshot\n"
                   "Sym+key: + T  - R  * 8  / N  ^ 6\n"
                   "  ( 9  ) 0  . B  , V  = Y  ! 1";
        case PageId::Functions:
            return "#63e2b7 Y= FUNCTIONS#\n"
                   "Type an expression in x, e.g.\n"
                   "  sin(x)   x^2-2   f   y=2x+1\n"
                   "Enter      next row / plot\n"
                   "Fn+F / X   previous / next row\n"
                   "Esc        back to CALC\n"
                   "User functions from CALC work too:\n"
                   "  f(x)=x^3-x  then type  f";
        case PageId::Graph:
            return "#63e2b7 GRAPH#\n"
                   "F X Z C / arrows   pan\n"
                   "I or +   zoom in    O or -  zoom out\n"
                   "0  reset view   A  fit y   S  square\n"
                   "1-4  show / hide y1..y4   L  legend\n"
                   "\n"
                   "#63e2b7 TRACE#\n"
                   "T / Enter  trace on/off\n"
                   "Z C        move along the curve\n"
                   "F X        switch function\n"
                   "R  root near cursor\n"
                   "M  min / max near cursor\n"
                   "Esc  leave trace, again: CALC";
    }
    return "";
}

}  // namespace

CalcApp::CalcApp(cz::Runtime& rt)
    : rt_(rt),
      style_(Style::make(rt.dark_mode(), rt.assets())),
      ctx_{rt_, engine_, style_, [this] { dirty_ = true; }, [this](PageId id) { show(id); },
           [this] { refresh_status(); }} {
    load();
    build();
    save_timer_ = lv_timer_create(
        [](lv_timer_t* t) {
            auto* self = static_cast<CalcApp*>(lv_timer_get_user_data(t));
            if (self->dirty_) self->save();
        },
        2000, this);
}

CalcApp::~CalcApp() {
    if (save_timer_) lv_timer_delete(save_timer_);
    save();
    destroy();
}

Page& CalcApp::page(PageId id) {
    switch (id) {
        case PageId::Calc: return *calc_;
        case PageId::Functions: return *functions_;
        case PageId::Graph: return *graph_;
    }
    return *calc_;
}

void CalcApp::build() {
    lv_obj_t* screen = lv_screen_active();
    root_ = make_box(screen, 0, 0, kWidth, kHeight);
    lv_obj_set_style_bg_color(root_, style_.bg, 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);

    // status bar: tabs left, modes right
    auto* bar = make_box(root_, 0, 0, kWidth, kStatusHeight);
    lv_obj_set_style_bg_color(bar, style_.surface, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    int x = 6;
    for (int i = 0; i < 3; ++i) {
        tabs_[i] = make_label(bar, style_.ui10, style_.dim, kTabNames[i]);
        lv_obj_set_pos(tabs_[i], x, 2);
        lv_obj_update_layout(tabs_[i]);
        x += lv_obj_get_width(tabs_[i]) + 12;
    }
    mode_label_ = make_label(bar, style_.ui10, style_.warn);
    lv_obj_align(mode_label_, LV_ALIGN_TOP_RIGHT, -6, 2);

    calc_ = std::make_unique<CalcPage>(ctx_);
    functions_ = std::make_unique<FunctionsPage>(ctx_);
    const auto view = graph_ ? graph_->viewport() : calc::Viewport::square(-10, 10, GraphPage::kW, GraphPage::kH);
    graph_ = std::make_unique<GraphPage>(ctx_);
    graph_->set_viewport(view);

    for (int i = 0; i < 3; ++i) {
        pages_[i] = make_box(root_, 0, kStatusHeight, kWidth, kHeight - kStatusHeight);
        page(static_cast<PageId>(i)).build(pages_[i]);
    }

    help_ = make_box(root_, 0, 0, kWidth, kHeight);
    lv_obj_set_style_bg_color(help_, style_.bg, 0);
    lv_obj_set_style_bg_opa(help_, LV_OPA_COVER, 0);
    lv_obj_add_flag(help_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(help_, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_set_style_pad_all(help_, 6, 0);
    help_text_ = make_label(help_, style_.mono12, style_.text);
    lv_label_set_recolor(help_text_, true);
    lv_obj_add_flag(help_, LV_OBJ_FLAG_HIDDEN);

    show(current_);
}

void CalcApp::destroy() {
    calc_.reset();
    functions_.reset();
    // keep graph_ alive across rebuilds only for its viewport; widgets die with root_
    if (root_) lv_obj_delete(root_);
    root_ = nullptr;
}

void CalcApp::show(PageId id) {
    current_ = id;
    for (int i = 0; i < 3; ++i) {
        if (i == static_cast<int>(id)) lv_obj_clear_flag(pages_[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(pages_[i], LV_OBJ_FLAG_HIDDEN);
    }
    page(id).on_show();
    refresh_status();
}

void CalcApp::refresh_status() {
    for (int i = 0; i < 3; ++i) {
        const bool active = i == static_cast<int>(current_);
        lv_obj_set_style_text_color(tabs_[i], active ? style_.accent : style_.dim, 0);
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%s  %s", engine_.env.angle == calc::AngleMode::Deg ? "DEG" : "RAD",
                  calc::format_name(engine_.format));
    lv_label_set_text(mode_label_, buf);
}

void CalcApp::toggle_help() {
    if (!lv_obj_has_flag(help_, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_add_flag(help_, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_label_set_text(help_text_, help_for(current_));
    lv_obj_scroll_to_y(help_, 0, LV_ANIM_OFF);
    lv_obj_clear_flag(help_, LV_OBJ_FLAG_HIDDEN);
}

void CalcApp::scroll_help(int dir) { lv_obj_scroll_by_bounded(help_, 0, -dir * 36, LV_ANIM_OFF); }

bool CalcApp::on_key(const cz::KeyEvent& ev) {
    const bool press = ev.phase == cz::KeyPhase::Press;
    const uint32_t k = ev.key;

    // help overlay owns input while visible
    if (!lv_obj_has_flag(help_, LV_OBJ_FLAG_HIDDEN)) {
        if (!ev.pressed_or_repeat()) return true;
        if (k == key::Up || k == 'f') scroll_help(1);
        else if (k == key::Down || k == 'x') scroll_help(-1);
        else if (press && (k == key::Esc || k == key::Help || k == key::F(1) || k == 'q')) toggle_help();
        return true;
    }

    if (press) {
        switch (k) {
            case key::Help:
            case key::F(1):
                toggle_help();
                return true;
            case key::Tab:
                show(static_cast<PageId>((static_cast<int>(current_) + 1) % 3));
                return true;
            case key::ShiftTab:
                show(static_cast<PageId>((static_cast<int>(current_) + 2) % 3));
                return true;
            case key::F(2):
                engine_.env.angle = engine_.env.angle == calc::AngleMode::Deg ? calc::AngleMode::Rad : calc::AngleMode::Deg;
                dirty_ = true;
                refresh_status();
                page(current_).on_engine_changed();
                return true;
            case key::F(3):
                engine_.format = engine_.format == calc::NumberFormat::Auto  ? calc::NumberFormat::Sci
                                 : engine_.format == calc::NumberFormat::Sci ? calc::NumberFormat::Eng
                                                                             : calc::NumberFormat::Auto;
                dirty_ = true;
                refresh_status();
                page(current_).on_engine_changed();
                return true;
            case key::F(4): {
                save();
                rt_.set_dark_mode(!rt_.dark_mode());
                style_ = Style::make(rt_.dark_mode(), rt_.assets());
                destroy();
                build();
                return true;
            }
            case key::F(5):
                engine_.history.clear();
                dirty_ = true;
                calc_->on_engine_changed();
                rt_.toast().show("History cleared", view::widgets::ToastTone::Default, 1200);
                return true;
            default:
                break;
        }
    }
    return page(current_).on_key(ev);
}

void CalcApp::load() {
    const auto path = rt_.state_dir() / "state.txt";
    std::ifstream in(path);
    if (!in) return;
    std::stringstream buf;
    buf << in.rdbuf();
    const std::string data = buf.str();
    engine_.deserialize(data);

    // graph viewport line (ignored by the engine)
    std::istringstream lines(data);
    std::string line;
    while (std::getline(lines, line)) {
        double v[4];
        if (std::sscanf(line.c_str(), "view\t%lf\t%lf\t%lf\t%lf", &v[0], &v[1], &v[2], &v[3]) == 4 &&
            v[1] > v[0] && v[3] > v[2]) {
            graph_ = std::make_unique<GraphPage>(ctx_);
            graph_->set_viewport({v[0], v[1], v[2], v[3]});
        }
    }
    LOG_INFO("loaded state: {} history entries", engine_.history.size());
}

void CalcApp::save() {
    dirty_ = false;
    const auto path = rt_.state_dir() / "state.txt";
    const auto tmp = path.string() + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) {
            LOG_WARN("cannot write {}", tmp);
            return;
        }
        out << engine_.serialize();
        if (graph_) {
            const auto v = graph_->viewport();
            char line[160];
            std::snprintf(line, sizeof(line), "view\t%.17g\t%.17g\t%.17g\t%.17g\n", v.xmin, v.xmax, v.ymin, v.ymax);
            out << line;
        }
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);  // atomic replace
    if (ec) LOG_WARN("cannot save {}: {}", path.string(), ec.message());
}

}  // namespace calc_ui
