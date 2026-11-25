// Which key layer the built-in keyboard is in: the CardputerZero keyboard makes Shift,
// Fn and Sym sticky (tap = next key only, double tap = locked) inside the driver, so
// evdev does not show it. The state is mirrored in the M5IOE1 expander as the modifier
// LED modes (I2C 0x4F: 0xBD Shift/caps, 0xBE Fn, 0xBF Sym/tables; 0 off, 1 once, 3 locked),
// read here with single SMBus byte reads (read-only, safe next to the kernel driver).
//
// Desktop: SDL modifiers stand in (Shift = Shift, Ctrl = Fn, Alt = Sym).
#pragma once

#include <cstdint>

namespace platform {

enum class KeyLayer { Base, Shift, Sym, Fn };

struct LayerState {
    KeyLayer layer{KeyLayer::Base};
    bool locked{false};
    bool operator==(const LayerState& o) const { return layer == o.layer && locked == o.locked; }
    bool operator!=(const LayerState& o) const { return !(*this == o); }
};

const char* layer_name(KeyLayer layer);  // "", "SHIFT", "SYM", "FN"

// raw register values -> layer (Fn wins over Sym, Sym over Shift)
LayerState layer_from_registers(uint8_t shift, uint8_t fn, uint8_t sym);

class ModifierWatch {
public:
    ModifierWatch();
    ~ModifierWatch();
    ModifierWatch(const ModifierWatch&) = delete;
    ModifierWatch& operator=(const ModifierWatch&) = delete;

    LayerState read();  // current state (cheap: three byte reads)
    // scripted runs / screenshots: pretend a layer until clear_force()
    void force(LayerState s) {
        forced_ = s;
        has_forced_ = true;
    }
    void clear_force() { has_forced_ = false; }

private:
    int fd_{-1};
    bool has_forced_{false};
    LayerState forced_;
};

}  // namespace platform
