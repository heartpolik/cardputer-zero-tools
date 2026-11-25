// A pretend serial line for the desktop simulator and the README screenshots:
// two Modbus RTU devices (a battery BMS at #1, an inverter at #16) and a modem
// that answers AT commands. Pure C++, deterministic for a given time.
#pragma once

#include "capture.h"

namespace ser {

class SimBus {
public:
    static constexpr uint8_t kBms = 1;
    static constexpr uint8_t kInverter = 16;

    // the reply to what was sent at now_us (empty: nobody answers)
    Bytes respond(const Bytes& sent, uint64_t now_us);

private:
    Bytes modbus(const Bytes& req, uint64_t now_us);
    bool reg(uint8_t addr, uint16_t index, uint64_t now_us, uint16_t& value) const;

    std::string line_;  // text typed so far (modem)
    uint16_t settings_[10]{100, 200, 300, 0, 0, 0, 0, 0, 0, 0};  // BMS holding 100..109, writable
};

}  // namespace ser
