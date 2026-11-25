#include "sim_bus.h"

#include "modbus.h"

#include <cstring>

namespace ser {
namespace {

void put16(Bytes& b, uint16_t v) {
    b.push_back(static_cast<uint8_t>(v >> 8));
    b.push_back(static_cast<uint8_t>(v & 0xff));
}

Bytes exception(uint8_t addr, uint8_t fc, uint8_t code) {
    Bytes b{addr, static_cast<uint8_t>(fc | 0x80), code};
    modbus::append_crc(b);
    return b;
}

Bytes text(const char* s) { return Bytes(s, s + std::strlen(s)); }

// a slow wobble so polling shows live values: -range..range over ~8 s
int wobble(uint64_t now_us, int range) {
    const int phase = static_cast<int>((now_us / 100000) % 80);  // 0..79
    const int tri = phase < 40 ? phase : 80 - phase;              // 0..40..0
    return (tri - 20) * range / 20;
}

}  // namespace

bool SimBus::reg(uint8_t addr, uint16_t i, uint64_t now_us, uint16_t& v) const {
    if (addr == kBms) {
        static const uint16_t cells[16] = {3318, 3321, 3320, 3319, 3322, 3318, 3317, 3321,
                                           3320, 3323, 3319, 3318, 3321, 3320, 3322, 3316};
        if (i == 0) v = static_cast<uint16_t>(5312 + wobble(now_us, 4));               // pack V x0.01
        else if (i == 1) v = static_cast<uint16_t>(static_cast<int16_t>(-125 + wobble(now_us, 30)));  // A x0.1
        else if (i == 2) v = 87;                                                        // SOC %
        else if (i == 3) v = 98;                                                        // SOH %
        else if (i == 4) v = 245;                                                       // temp x0.1 C
        else if (i == 5) v = 132;                                                       // cycles
        else if (i >= 6 && i < 22) v = cells[i - 6];                                    // cell mV
        else if (i == 22 || i == 23) {                                                  // capacity F32 CDAB
            const float cap = 314.0f;
            uint32_t bits;
            std::memcpy(&bits, &cap, sizeof bits);
            v = i == 22 ? static_cast<uint16_t>(bits & 0xffff) : static_cast<uint16_t>(bits >> 16);
        } else if (i >= 24 && i < 40) v = 0;
        else if (i >= 100 && i < 110) v = settings_[i - 100];
        else return false;
        return true;
    }
    if (addr == kInverter) {
        static const uint16_t inv[8] = {2301, 5000, 842, 532, 1210, 3, 0, 0};
        if (i >= 8) return false;
        v = inv[i];
        if (i == 2) v = static_cast<uint16_t>(842 + wobble(now_us, 60));
        return true;
    }
    return false;
}

Bytes SimBus::modbus(const Bytes& q, uint64_t now_us) {
    const uint8_t addr = q[0], fc = q[1];
    if (addr != kBms && addr != kInverter) return {};
    if (q.size() != 8 && fc != modbus::kWriteRegisters) return exception(addr, fc, 3);
    const uint16_t start = static_cast<uint16_t>(q[2] << 8 | q[3]);
    const uint16_t count = static_cast<uint16_t>(q[4] << 8 | q[5]);

    switch (fc) {
        case modbus::kReadHolding:
        case modbus::kReadInput: {
            if (count == 0 || count > 125) return exception(addr, fc, 3);
            Bytes r{addr, fc, static_cast<uint8_t>(count * 2)};
            for (uint16_t k = 0; k < count; ++k) {
                uint16_t v;
                if (!reg(addr, static_cast<uint16_t>(start + k), now_us, v)) return exception(addr, fc, 2);
                put16(r, v);
            }
            modbus::append_crc(r);
            return r;
        }
        case modbus::kReadCoils:
        case modbus::kReadDiscrete: {
            if (addr != kBms) return exception(addr, fc, 1);
            if (count == 0 || start + count > 16) return exception(addr, fc, 2);
            const uint16_t pattern = 0b0000001100100101;  // charge, discharge, balance... flags
            Bytes r{addr, fc, static_cast<uint8_t>((count + 7) / 8)};
            for (int byte = 0; byte < (count + 7) / 8; ++byte) {
                uint8_t v = 0;
                for (int b = 0; b < 8 && byte * 8 + b < count; ++b) {
                    if ((pattern >> (start + byte * 8 + b)) & 1) v = static_cast<uint8_t>(v | 1 << b);
                }
                r.push_back(v);
            }
            modbus::append_crc(r);
            return r;
        }
        case modbus::kWriteRegister: {
            if (addr != kBms || start < 100 || start >= 110) return exception(addr, fc, 2);
            settings_[start - 100] = count;  // the value sits where a count would
            return q;
        }
        default:
            return exception(addr, fc, 1);
    }
}

Bytes SimBus::respond(const Bytes& sent, uint64_t now_us) {
    if (sent.size() >= 4 && modbus::crc_ok(sent)) return modbus(sent, now_us);

    // the modem: collect a line, answer on CR or LF
    Bytes out;
    for (uint8_t c : sent) {
        if (c != '\r' && c != '\n') {
            line_ += static_cast<char>(c);
            continue;
        }
        if (line_.empty()) continue;
        std::string cmd;
        for (char ch : line_) cmd += static_cast<char>(ch >= 'a' && ch <= 'z' ? ch - 32 : ch);
        line_.clear();
        Bytes r;
        if (cmd == "AT") r = text("OK\r\n");
        else if (cmd == "ATI") r = text("SerialPuter SIM modem 1.0\r\nOK\r\n");
        else if (cmd == "AT+CSQ") r = text("+CSQ: 21,0\r\nOK\r\n");
        else if (cmd.rfind("AT", 0) == 0) r = text("ERROR\r\n");
        else r = text(("echo: " + cmd + "\r\n").c_str());
        out.insert(out.end(), r.begin(), r.end());
    }
    return out;
}

}  // namespace ser
