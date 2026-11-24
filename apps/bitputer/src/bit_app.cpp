#include "bit_app.h"

#include "logger.h"

#include <fstream>
#include <sstream>

namespace bit_ui {
namespace {

namespace key = platform::key;

constexpr const char* kTabNames[2] = {"BITS", "CRC"};

const char* help_for(PageId id) {
    switch (id) {
        case PageId::Calc:
            return "#63e2b7 BITS#\n"
                   "Enter     evaluate -> ans (empty input: ans)\n"
                   "Fn+F/X    history     Esc clear (hold: quit)\n"
                   "&0xF0     leading operator continues ans\n"
                   "x = 0x10  variable\n"
                   "\n"
                   "#63e2b7 BIT GRID# (Fn+X from the input)\n"
                   "Z C / , .  move    F X  row (64-bit)\n"
                   "Space      flip    0 1  clear / set\n"
                   "I          invert  Esc  back to input\n"
                   "\n"
                   "#63e2b7 OPERATORS# (C precedence)\n"
                   "| ^ &  << >> (arith. in signed) >>>\n"
                   "+ - * / %   ~x  -x   a ** b (power)\n"
                   "\n"
                   "#63e2b7 LITERALS#\n"
                   "255 0xFF 0o377 0b1111 0n255 'A' 'RIFF'\n"
                   "1_000  4K 2M 1G   f32(1.5)  f64(0.1)\n"
                   "HEX mode: ff = 255, 0b1 is hex\n"
                   "\n"
                   "#63e2b7 FUNCTIONS#\n"
                   "popcnt clz ctz parity bswap rev\n"
                   "bit(n) mask(n) mask(hi,lo)\n"
                   "field(x,hi,lo) set clr tgl (x,n)\n"
                   "rol ror (x,n)  sext zext (x,bits)\n"
                   "u8 u16 u32 i8 i16 i32  abs min max\n"
                   "align(x,a) isqrt log2 gcd\n"
                   "\n"
                   "#63e2b7 MODES#\n"
                   "F2 input base DEC/HEX/BIN/OCT\n"
                   "F3 width 8/16/32/64   F4 signed\n"
                   "F5 CHR / FLT / UTC row   F6 theme\n"
                   "Tab  BITS <> CRC    Fn+J screenshot\n"
                   "Sym = blue legends: & | ^ ~ < > ( )";
        case PageId::Crc:
            return "#63e2b7 CRC / CHECKSUMS#\n"
                   "Bytes:  01 03 00 00 00 0A\n"
                   "        0x01,0x03  010300  (any sep.)\n"
                   "Text:   \"AT\\r\\n\"  escapes \\n \\r \\t \\xHH\n"
                   "\n"
                   "Fn+F/X  select    Enter  value to BITS\n"
                   "Esc     clear input\n"
                   "wire = bytes in the order they are sent\n"
                   "(Modbus, Kermit, ARC: low byte first)\n"
                   "\n"
                   "MODBUS  32  32C  CCITT  XMODEM\n"
                   "KERMIT  ARC  CRC-8  MAXIM (1-Wire)\n"
                   "NRSC-5 (Sensirion)  XOR8 (NMEA)  SUM8";
    }
    return "";
}

}  // namespace

BitApp::BitApp(cz::Runtime& rt)
    : rt_(rt),
      style_(Style::make(rt.dark_mode(), rt.assets())),
      ctx_{rt_, session_, style_, [this] { dirty_ = true; }, [this](PageId id) { show(id); },
           [this](const std::string& text) {
               calc_->set_input(text);
               show(PageId::Calc);
           }} {
    load();
    build();
    save_timer_ = lv_timer_create(
        [](lv_timer_t* t) {
            auto* self = static_cast<BitApp*>(lv_timer_get_user_data(t));
            if (self->dirty_) self->save();
        },
        2000, this);
}

BitApp::~BitApp() {
    if (save_timer_) lv_timer_delete(save_timer_);
    save();
    destroy();
}

Page& BitApp::page(PageId id) {
    if (id == PageId::Crc) return *crc_;
    return *calc_;
}

void BitApp::build() {
    root_ = make_box(lv_screen_active(), 0, 0, kWidth, kHeight);
    lv_obj_set_style_bg_color(root_, style_.bg, 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);

    auto* bar = make_box(root_, 0, 0, kWidth, kStatusHeight);
    lv_obj_set_style_bg_color(bar, style_.surface, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    int x = 6;
    for (int i = 0; i < 2; ++i) {
        tabs_[i] = make_label(bar, style_.ui10, style_.dim, kTabNames[i]);
        lv_obj_set_pos(tabs_[i], x, 2);
        lv_obj_update_layout(tabs_[i]);
        x += lv_obj_get_width(tabs_[i]) + 12;
    }
    mode_label_ = make_label(bar, style_.ui10, style_.warn);
    lv_obj_align(mode_label_, LV_ALIGN_TOP_RIGHT, -6, 2);

    calc_ = std::make_unique<CalcPage>(ctx_);
    crc_ = std::make_unique<CrcPage>(ctx_);
    for (int i = 0; i < 2; ++i) {
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

void BitApp::destroy() {
    calc_.reset();
    crc_.reset();
    if (root_) lv_obj_delete(root_);
    root_ = nullptr;
}

void BitApp::show(PageId id) {
    current_ = id;
    for (int i = 0; i < 2; ++i) {
        if (i == static_cast<int>(id)) lv_obj_clear_flag(pages_[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(pages_[i], LV_OBJ_FLAG_HIDDEN);
    }
    page(id).on_show();
    refresh_status();
}

void BitApp::refresh_status() {
    for (int i = 0; i < 2; ++i) {
        lv_obj_set_style_text_color(tabs_[i], i == static_cast<int>(current_) ? style_.accent : style_.dim, 0);
    }
    const auto& env = session_.env;
    lv_label_set_text_fmt(mode_label_, "IN %s   %c%d", bits::base_name(env.input), env.word.is_signed ? 'i' : 'u',
                          env.word.width);
}

void BitApp::modes_changed() {
    dirty_ = true;
    refresh_status();
    calc_->on_mode_changed();
    crc_->on_mode_changed();
}

void BitApp::toggle_help() {
    if (!lv_obj_has_flag(help_, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_add_flag(help_, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_label_set_text(help_text_, help_for(current_));
    lv_obj_scroll_to_y(help_, 0, LV_ANIM_OFF);
    lv_obj_clear_flag(help_, LV_OBJ_FLAG_HIDDEN);
}

bool BitApp::on_key(const cz::KeyEvent& ev) {
    const bool press = ev.phase == cz::KeyPhase::Press;
    const uint32_t k = ev.key;

    if (!lv_obj_has_flag(help_, LV_OBJ_FLAG_HIDDEN)) {
        if (!ev.pressed_or_repeat()) return true;
        if (k == key::Up || k == 'f') lv_obj_scroll_by_bounded(help_, 0, 36, LV_ANIM_OFF);
        else if (k == key::Down || k == 'x') lv_obj_scroll_by_bounded(help_, 0, -36, LV_ANIM_OFF);
        else if (press && (k == key::Esc || k == key::Help || k == key::F(1) || k == 'q')) toggle_help();
        return true;
    }

    // pages see Tab first (the bit grid leaves on Tab), then the shell switches
    if (press) {
        auto& env = session_.env;
        switch (k) {
            case key::Help:
            case key::F(1):
                toggle_help();
                return true;
            case key::Tab:
            case key::ShiftTab:
                page(current_).on_key(ev);
                show(current_ == PageId::Calc ? PageId::Crc : PageId::Calc);
                return true;
            case key::F(2):
                env.input = bits::next_base(env.input);
                modes_changed();
                return true;
            case key::F(3):
                env.word.width = bits::next_width(env.word.width);
                modes_changed();
                return true;
            case key::F(4):
                env.word.is_signed = !env.word.is_signed;
                modes_changed();
                return true;
            case key::F(5):
                session_.decode = bits::next_decode(session_.decode);
                modes_changed();
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

void BitApp::load() {
    std::ifstream in(rt_.state_dir() / "state.txt");
    if (!in) return;
    std::stringstream buf;
    buf << in.rdbuf();
    session_.deserialize(buf.str());
    LOG_INFO("loaded state: {} history entries", session_.history.size());
}

void BitApp::save() {
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

}  // namespace bit_ui
