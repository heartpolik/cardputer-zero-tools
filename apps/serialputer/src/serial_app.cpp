#include "serial_app.h"

#include "logger.h"

#include <fstream>
#include <sstream>

namespace ser_ui {
namespace {

namespace key = platform::key;

constexpr const char* kTabNames[kPageCount] = {"TERM", "MODBUS", "PORT"};

const char* help_for(PageId id) {
    switch (id) {
        case PageId::Term:
            return "#63e2b7 TERMINAL#\n"
                   "<  received    >  sent (yellow)\n"
                   "Enter   send   (empty line: repeat last)\n"
                   "Fn+F/X  history       Esc clear input\n"
                   "\n"
                   "#63e2b7 SENDING# (F3 TXT / HEX)\n"
                   "TXT  AT+CSQ   escapes \\r \\n \\t \\xHH\n"
                   "     F4 line end: none CR LF CRLF\n"
                   "HEX  01 03 00 00 00 0A  or \"text\"\n"
                   "     F4 add Modbus CRC\n"
                   "\n"
                   "#63e2b7 VIEW#\n"
                   "F2  HEX / TXT / DUMP (hex + ASCII)\n"
                   "F5  time: off / since start / delta\n"
                   "F7  hold: scroll back with Fn+F/X\n"
                   "F8  clear      F9 log to file\n"
                   "F10 Modbus labels (#1 READ HOLDING..)\n"
                   "\n"
                   "#63e2b7 FRAMES#\n"
                   "A pause on the line ends a frame (gap),\n"
                   "or LF does (line mode): PORT > Split.\n"
                   "\n"
                   "#63e2b7 WIRING# (3.3 V logic!)\n"
                   "module TX > CZ RX, module RX < CZ TX, GND\n"
                   "RS485: A+ to A+, B- to B-\n"
                   "\n"
                   "Tab TERM > MODBUS > PORT   F6 theme";
        case PageId::Modbus:
            return "#63e2b7 MODBUS RTU MASTER#\n"
                   "Fn+Z/C  field: ID FN REG N\n"
                   "0-9     type a value   Bksp erase\n"
                   "Space   on FN: next function\n"
                   "FN 01 coils  02 inputs\n"
                   "   03 holding  04 input registers\n"
                   "\n"
                   "Enter   read once\n"
                   "F3      poll (PORT > Poll every)\n"
                   "F5      scan addresses (PORT > Scan)\n"
                   "F2      value: U16 I16 HEX U32 I32 F32\n"
                   "        ABCD / CDAB = word order\n"
                   "Fn+F/X  scroll the table\n"
                   "F8      reset counters   Esc stop\n"
                   "\n"
                   "REG is the 0-based address on the wire:\n"
                   "40001 in a manual = holding REG 0,\n"
                   "30001 = input REG 0.\n"
                   "\n"
                   "Only reads: nothing is written to\n"
                   "the device.";
        case PageId::Port:
            return "#63e2b7 PORT#\n"
                   "Fn+F/X  choose a row\n"
                   "Fn+Z/C  change     Enter open/close\n"
                   "\n"
                   "SIM     demo bus: BMS #1, inverter #16,\n"
                   "        modem (AT, ATI, AT+CSQ)\n"
                   "serial0 header UART (ttyAMA0)\n"
                   "ttyUSB* / ttyACM*  USB adapters\n"
                   "\n"
                   "Split   gap auto = Modbus 3.5 chars\n"
                   "        (at least 2 ms), or fixed;\n"
                   "        line = LF ends a frame\n"
                   "Poll / Timeout / Scan: Modbus page\n"
                   "Log     ~/.config/cz-serialputer/logs/";
    }
    return "";
}

}  // namespace

SerialApp::SerialApp(cz::Runtime& rt)
    : rt_(rt),
      style_(Style::make(rt.dark_mode(), rt.assets())),
      engine_(std::make_unique<Engine>(session_, rt.state_dir() / "logs")),
      ctx_{rt_,
           session_,
           *engine_,
           style_,
           [this] { dirty_ = true; },
           [this] { refresh_status(); },
           [this](const std::string& text) {
               rt_.toast().show(text, view::widgets::ToastTone::Default, 1800);
           }} {
    load();
    engine_->apply_format();
    // open the last port if it is still there (SIM always is)
    if (session_.port == ser::kSimPort || std::filesystem::exists(session_.port)) engine_->open();
    build();
    io_timer_ = lv_timer_create([](lv_timer_t* t) { static_cast<SerialApp*>(lv_timer_get_user_data(t))->engine_->tick(); },
                                2, this);
    ui_timer_ = lv_timer_create(
        [](lv_timer_t* t) {
            auto* self = static_cast<SerialApp*>(lv_timer_get_user_data(t));
            self->page(self->current_).tick();
            self->refresh_status();
        },
        50, this);
    save_timer_ = lv_timer_create(
        [](lv_timer_t* t) {
            auto* self = static_cast<SerialApp*>(lv_timer_get_user_data(t));
            if (self->dirty_) self->save();
        },
        2000, this);
}

SerialApp::~SerialApp() {
    for (auto* t : {io_timer_, ui_timer_, save_timer_}) {
        if (t) lv_timer_delete(t);
    }
    save();
    destroy();
    engine_.reset();
}

Page& SerialApp::page(PageId id) {
    switch (id) {
        case PageId::Modbus: return *modbus_;
        case PageId::Port: return *port_;
        default: return *term_;
    }
}

void SerialApp::build() {
    root_ = make_box(lv_screen_active(), 0, 0, kWidth, kHeight);
    lv_obj_set_style_bg_color(root_, style_.bg, 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);

    auto* bar = make_box(root_, 0, 0, kWidth, kStatusHeight);
    lv_obj_set_style_bg_color(bar, style_.surface, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    int x = 6;
    for (int i = 0; i < kPageCount; ++i) {
        tabs_[i] = make_label(bar, style_.ui10, style_.dim, kTabNames[i]);
        lv_obj_set_pos(tabs_[i], x, 2);
        lv_obj_update_layout(tabs_[i]);
        x += lv_obj_get_width(tabs_[i]) + 10;
    }
    port_label_ = make_label(bar, style_.ui10, style_.warn);
    lv_label_set_recolor(port_label_, true);
    lv_obj_align(port_label_, LV_ALIGN_TOP_RIGHT, -6, 2);

    term_ = std::make_unique<TermPage>(ctx_);
    modbus_ = std::make_unique<ModbusPage>(ctx_);
    port_ = std::make_unique<PortPage>(ctx_);
    for (int i = 0; i < kPageCount; ++i) {
        pages_[i] = make_box(root_, 0, kStatusHeight, kWidth, kContentH);
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

void SerialApp::destroy() {
    term_.reset();
    modbus_.reset();
    port_.reset();
    if (root_) lv_obj_delete(root_);
    root_ = nullptr;
}

void SerialApp::show(PageId id) {
    current_ = id;
    for (int i = 0; i < kPageCount; ++i) {
        if (i == static_cast<int>(id)) lv_obj_clear_flag(pages_[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(pages_[i], LV_OBJ_FLAG_HIDDEN);
    }
    page(id).on_show();
    refresh_status();
}

void SerialApp::refresh_status() {
    if (!port_label_) return;
    for (int i = 0; i < kPageCount; ++i) {
        lv_obj_set_style_text_color(tabs_[i], i == static_cast<int>(current_) ? style_.accent : style_.dim, 0);
    }
    const auto& eng = *engine_;
    std::string name = session_.port;
    const auto slash = name.rfind('/');
    if (slash != std::string::npos) name = name.substr(slash + 1);
    const std::string state = eng.is_open() ? "#" + color_hex(style_.accent) + " ON#" : "#" + color_hex(style_.error) + " OFF#";
    std::string flags;
    if (eng.logging()) flags += " LOG";
    if (eng.polling()) flags += " POLL";
    if (eng.scanning()) flags += " SCAN";
    char buf[160];
    std::snprintf(buf, sizeof buf, "%s%s %s %d %s", flags.c_str(), flags.empty() ? "" : " ", name.c_str(),
                  session_.fmt.baud, ser::frame_name(session_.fmt).c_str());
    const std::string text = std::string(buf) + " " + state;
    if (text != lv_label_get_text(port_label_)) lv_label_set_text(port_label_, text.c_str());
}

void SerialApp::toggle_help() {
    if (!lv_obj_has_flag(help_, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_add_flag(help_, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_label_set_text(help_text_, help_for(current_));
    lv_obj_scroll_to_y(help_, 0, LV_ANIM_OFF);
    lv_obj_clear_flag(help_, LV_OBJ_FLAG_HIDDEN);
}

bool SerialApp::on_key(const cz::KeyEvent& ev) {
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
                show(static_cast<PageId>((static_cast<int>(current_) + 1) % kPageCount));
                return true;
            case key::ShiftTab:
                show(static_cast<PageId>((static_cast<int>(current_) + kPageCount - 1) % kPageCount));
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

void SerialApp::load() {
    std::ifstream in(rt_.state_dir() / "state.txt");
    if (!in) return;
    std::stringstream buf;
    buf << in.rdbuf();
    session_.deserialize(buf.str());
    LOG_INFO("loaded state: port {}", session_.port);
}

void SerialApp::save() {
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

}  // namespace ser_ui
