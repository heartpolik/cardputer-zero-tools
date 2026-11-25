#include "ohm_app.h"

#include "logger.h"

#include <fstream>
#include <sstream>

namespace ohm_ui {
namespace {

namespace key = platform::key;

constexpr const char* kTabNames[kPages] = {"MARK", "BANDS", "TOOLS"};

const char* help_for(PageId id) {
    switch (id) {
        case PageId::Mark:
            return "#63e2b7 MARK# - read a printed code\n"
                   "Resistors   103 = 10k   4992 = 49.9k\n"
                   "            4R7  R47  4K7  1M0 (RKM)\n"
                   "            01C = EIA-96 (1 %)\n"
                   "Capacitors  104 = 100n  229 = 2.2p\n"
                   "            104K = +-10 %  2A104J = 100 V\n"
                   "            4n7  2p2  u47 (RKM)\n"
                   "Tantalum    A106 = 10u 10 V\n"
                   "Inductors   101 = 100u  4R7 = 4.7u\n"
                   "Parts       M1-M7 T4 A7 1AM 2A J3Y ...\n"
                   "\n"
                   "Every plausible reading is listed;\n"
                   "resistors also show their colour code.\n"
                   "Part codes vary by maker: check the\n"
                   "package and the datasheet.";
        case PageId::Bands:
            return "#63e2b7 BANDS#\n"
                   "Type the colours from the left end:\n"
                   "0 black 1 brown 2 red 3 orange\n"
                   "4 yellow 5 green 6 blue 7 violet\n"
                   "8 grey 9 white  G gold  S silver\n"
                   "(the digit each colour stands for)\n"
                   "3-6 bands; a gold/silver first band is\n"
                   "read from the other end.\n"
                   "BS undo   Esc clear\n"
                   "\n"
                   "Space: value -> bands (4k7, 4.99k)";
        case PageId::Tools:
            return "#63e2b7 TOOLS#\n"
                   "Fn+Z / Fn+C   previous / next tool\n"
                   "Fn+F / Fn+X   field     Enter next\n"
                   "Type a value: 4k7 2.2u 10m 1e-3 3V3\n"
                   "Esc / Del clear the field\n"
                   "\n"
                   "Fill any inputs, the rest is computed\n"
                   "(green). Typing one more input clears\n"
                   "the oldest one.\n"
                   "Ohm: V I R P (any two)\n"
                   "Divider: 3 of Vin R1 R2 Vout, or\n"
                   "  Vin + Vout -> best resistor pair\n"
                   "LED: supply Vf If -> R, rating\n"
                   "RC: tau / cut-off   LC: resonance\n"
                   "Battery: Ah V load eff DoD -> runtime\n"
                   "\n"
                   "F2 E-series E6..E96   F6 theme";
    }
    return "";
}

}  // namespace

OhmApp::OhmApp(cz::Runtime& rt)
    : rt_(rt),
      style_(Style::make(rt.dark_mode(), rt.assets())),
      ctx_{rt_, session_, style_, [this] { dirty_ = true; }, [this] { refresh_status(); }} {
    load();
    build();
    rt_.set_layer_handler([this](cz::LayerState s) {
        refresh_layer_bar();  // labels can depend on modes and the page
        layer_bar_.show_layer(s);
    });
    save_timer_ = lv_timer_create(
        [](lv_timer_t* t) {
            auto* self = static_cast<OhmApp*>(lv_timer_get_user_data(t));
            if (self->dirty_) self->save();
        },
        2000, this);
}

OhmApp::~OhmApp() {
    rt_.set_layer_handler(nullptr);
    if (save_timer_) lv_timer_delete(save_timer_);
    save();
    destroy();
}

Page& OhmApp::page(PageId id) {
    switch (id) {
        case PageId::Mark: return *mark_;
        case PageId::Bands: return *bands_;
        case PageId::Tools: return *tools_;
    }
    return *mark_;
}

void OhmApp::build() {
    root_ = make_box(lv_screen_active(), 0, 0, kWidth, kHeight);
    lv_obj_set_style_bg_color(root_, style_.bg, 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);

    auto* bar = make_box(root_, 0, 0, kWidth, kStatusHeight);
    lv_obj_set_style_bg_color(bar, style_.surface, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    int x = 6;
    for (int i = 0; i < kPages; ++i) {
        tabs_[i] = make_label(bar, style_.ui10, style_.dim, kTabNames[i]);
        lv_obj_set_pos(tabs_[i], x, 2);
        lv_obj_update_layout(tabs_[i]);
        x += lv_obj_get_width(tabs_[i]) + 12;
    }
    mode_label_ = make_label(bar, style_.ui10, style_.warn);
    lv_obj_align(mode_label_, LV_ALIGN_TOP_RIGHT, -6, 2);

    mark_ = std::make_unique<MarkPage>(ctx_);
    bands_ = std::make_unique<BandsPage>(ctx_);
    tools_ = std::make_unique<ToolsPage>(ctx_);
    for (int i = 0; i < kPages; ++i) {
        pages_[i] = make_box(root_, 0, kStatusHeight, kWidth, kContentH);
        page(static_cast<PageId>(i)).build(pages_[i]);
    }

    layer_bar_ = cz::ShortcutBar{};
    layer_bar_.build(root_, style_);
    layer_bar_.set_auto_hide(true);
    refresh_layer_bar();
    layer_bar_.show_layer(rt_.key_layer());

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

void OhmApp::destroy() {
    mark_.reset();
    bands_.reset();
    tools_.reset();
    if (root_) lv_obj_delete(root_);
    root_ = nullptr;
    mode_label_ = nullptr;
}

void OhmApp::show(PageId id) {
    current_ = id;
    for (int i = 0; i < kPages; ++i) {
        if (i == static_cast<int>(id)) lv_obj_clear_flag(pages_[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(pages_[i], LV_OBJ_FLAG_HIDDEN);
    }
    page(id).on_show();
    refresh_status();
}

void OhmApp::refresh_status() {
    if (!mode_label_ || !pages_[kPages - 1]) return;  // still building
    for (int i = 0; i < kPages; ++i) {
        lv_obj_set_style_text_color(tabs_[i], i == static_cast<int>(current_) ? style_.accent : style_.dim, 0);
    }
    const char* extra = page(current_).status();
    lv_label_set_text_fmt(mode_label_, "%s%s%s", extra, *extra ? "   " : "", ohm::series_name(session_.series));
}

// what 4..8 do with Fn (F4..F8 = Fn + 4..8); Shift / Sym show the typed characters
void OhmApp::refresh_layer_bar() {
    layer_bar_.set(6, "theme", true, cz::KeyLayer::Fn);
}

void OhmApp::toggle_help() {
    if (!lv_obj_has_flag(help_, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_add_flag(help_, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_label_set_text(help_text_, help_for(current_));
    lv_obj_scroll_to_y(help_, 0, LV_ANIM_OFF);
    lv_obj_clear_flag(help_, LV_OBJ_FLAG_HIDDEN);
}

bool OhmApp::on_key(const cz::KeyEvent& ev) {
    const bool press = ev.phase == cz::KeyPhase::Press;
    const uint32_t k = ev.key;

    if (!lv_obj_has_flag(help_, LV_OBJ_FLAG_HIDDEN)) {
        if (!ev.pressed_or_repeat()) return true;
        if (k == key::Up || k == 'f') lv_obj_scroll_by_bounded(help_, 0, 36, LV_ANIM_OFF);
        else if (k == key::Down || k == 'x') lv_obj_scroll_by_bounded(help_, 0, -36, LV_ANIM_OFF);
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
                show(static_cast<PageId>((static_cast<int>(current_) + 1) % kPages));
                return true;
            case key::ShiftTab:
                show(static_cast<PageId>((static_cast<int>(current_) + kPages - 1) % kPages));
                return true;
            case key::F(2):
                session_.series = ohm::next_series(session_.series);
                dirty_ = true;
                for (int i = 0; i < kPages; ++i) page(static_cast<PageId>(i)).on_series_changed();
                refresh_status();
                return true;
            case key::F(6): {
                save();
                rt_.set_dark_mode(!rt_.dark_mode());
                style_ = Style::make(rt_.dark_mode(), rt_.assets());
                destroy();
                build();
                return true;
            }
            default:
                break;
        }
    }
    return page(current_).on_key(ev);
}

void OhmApp::load() {
    std::ifstream in(rt_.state_dir() / "state.txt");
    if (!in) return;
    std::stringstream buf;
    buf << in.rdbuf();
    session_.deserialize(buf.str());
    LOG_INFO("loaded state");
}

void OhmApp::save() {
    dirty_ = false;
    const auto path = rt_.state_dir() / "state.txt";
    const auto tmp = path.string() + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) {
            LOG_WARN("cannot write {}", tmp);
            return;
        }
        out << session_.serialize();
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    if (ec) LOG_WARN("cannot save {}: {}", path.string(), ec.message());
}

}  // namespace ohm_ui
