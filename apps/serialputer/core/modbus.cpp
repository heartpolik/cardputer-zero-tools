#include "modbus.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace ser::modbus {
namespace {

uint16_t be16(const Bytes& b, size_t i) { return static_cast<uint16_t>(b[i] << 8 | b[i + 1]); }

void put16(Bytes& b, uint16_t v) {
    b.push_back(static_cast<uint8_t>(v >> 8));
    b.push_back(static_cast<uint8_t>(v & 0xff));
}

std::string fmt(const char* f, ...) __attribute__((format(printf, 1, 2)));
std::string fmt(const char* f, ...) {
    char buf[160];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(buf, sizeof buf, f, ap);
    va_end(ap);
    return buf;
}

std::vector<uint16_t> regs_from(const Bytes& b, size_t at, size_t bytes) {
    std::vector<uint16_t> r;
    for (size_t i = 0; i + 1 < bytes; i += 2) r.push_back(be16(b, at + i));
    return r;
}

}  // namespace

const char* fn_name(uint8_t fc) {
    switch (fc) {
        case kReadCoils: return "READ COILS";
        case kReadDiscrete: return "READ INPUTS";
        case kReadHolding: return "READ HOLDING";
        case kReadInput: return "READ INPUT REGS";
        case kWriteCoil: return "WRITE COIL";
        case kWriteRegister: return "WRITE REG";
        case kWriteCoils: return "WRITE COILS";
        case kWriteRegisters: return "WRITE REGS";
        default: return "";
    }
}

const char* exception_name(uint8_t code) {
    switch (code) {
        case 1: return "ILLEGAL FUNCTION";
        case 2: return "ILLEGAL DATA ADDRESS";
        case 3: return "ILLEGAL DATA VALUE";
        case 4: return "DEVICE FAILURE";
        case 5: return "ACKNOWLEDGE";
        case 6: return "DEVICE BUSY";
        case 8: return "MEMORY PARITY ERROR";
        case 10: return "GATEWAY PATH UNAVAILABLE";
        case 11: return "GATEWAY TARGET FAILED";
        default: return "UNKNOWN EXCEPTION";
    }
}

bool is_read(uint8_t fc) { return fc >= kReadCoils && fc <= kReadInput; }
bool is_bit_fn(uint8_t fc) { return fc == kReadCoils || fc == kReadDiscrete; }

uint8_t next_read_fn(uint8_t fc, int dir) {
    int f = is_read(fc) ? fc : kReadHolding;
    f = (f - 1 + dir + 4) % 4 + 1;
    return static_cast<uint8_t>(f);
}

uint16_t crc16(const uint8_t* data, size_t n) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < n; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) crc = (crc & 1) ? static_cast<uint16_t>((crc >> 1) ^ 0xA001) : crc >> 1;
    }
    return crc;
}

bool crc_ok(const Bytes& f) {
    if (f.size() < 4) return false;
    const uint16_t c = crc16(f.data(), f.size() - 2);
    return f[f.size() - 2] == (c & 0xff) && f[f.size() - 1] == (c >> 8);
}

void append_crc(Bytes& f) {
    const uint16_t c = crc16(f.data(), f.size());
    f.push_back(static_cast<uint8_t>(c & 0xff));  // low byte first
    f.push_back(static_cast<uint8_t>(c >> 8));
}

uint16_t max_count(uint8_t fc) { return is_bit_fn(fc) ? 2000 : 125; }

Bytes encode(const Request& r) {
    Bytes b{r.addr, r.fc};
    switch (r.fc) {
        case kReadCoils:
        case kReadDiscrete:
        case kReadHolding:
        case kReadInput:
            put16(b, r.start);
            put16(b, r.count);
            break;
        case kWriteCoil:
            put16(b, r.start);
            put16(b, !r.values.empty() && r.values[0] ? 0xFF00 : 0x0000);
            break;
        case kWriteRegister:
            put16(b, r.start);
            put16(b, r.values.empty() ? 0 : r.values[0]);
            break;
        case kWriteRegisters:
            put16(b, r.start);
            put16(b, static_cast<uint16_t>(r.values.size()));
            b.push_back(static_cast<uint8_t>(r.values.size() * 2));
            for (uint16_t v : r.values) put16(b, v);
            break;
        default:
            break;
    }
    append_crc(b);
    return b;
}

Response decode_response(const Request& req, const Bytes& f) {
    Response r;
    if (f.size() < 5) {
        r.error = "short frame";
        return r;
    }
    if (!crc_ok(f)) {
        r.error = "CRC error";
        return r;
    }
    if (f[0] != req.addr) {
        r.error = fmt("from #%u, asked #%u", f[0], req.addr);
        return r;
    }
    if (f[1] == (req.fc | 0x80)) {
        r.exception = true;
        r.code = f[2];
        r.error = exception_name(f[2]);
        return r;
    }
    if (f[1] != req.fc) {
        r.error = fmt("function %u, asked %u", f[1], req.fc);
        return r;
    }
    if (is_read(req.fc)) {
        const size_t bc = f[2];
        if (f.size() != bc + 5) {
            r.error = "byte count mismatch";
            return r;
        }
        if (is_bit_fn(req.fc)) {
            if (bc != (req.count + 7u) / 8) {
                r.error = "wrong number of bits";
                return r;
            }
            for (uint16_t i = 0; i < req.count; ++i) r.bits.push_back((f[3 + i / 8] >> (i % 8)) & 1);
        } else {
            if (bc != req.count * 2u) {
                r.error = fmt("%zu regs, asked %u", bc / 2, req.count);
                return r;
            }
            r.regs = regs_from(f, 3, bc);
        }
        r.ok = true;
        return r;
    }
    // writes answer with an echo of address/count
    if (f.size() != 8) {
        r.error = "unexpected length";
        return r;
    }
    r.ok = true;
    return r;
}

// ------------------------------------------------------------ sniffing

Description Sniffer::describe(const Bytes& f) {
    Description d;
    if (f.size() < 4) {
        d.text = "short frame";
        return d;
    }
    d.crc_ok = crc_ok(f);
    d.addr = f[0];
    d.fc = f[1];
    if (!d.crc_ok) {
        d.text = "not Modbus (CRC)";
        return d;
    }
    const uint8_t fc = static_cast<uint8_t>(f[1] & 0x7f);
    const char* name = fn_name(fc);

    if (f[1] & 0x80) {
        d.kind = Description::Kind::Exception;
        d.fc = fc;
        d.text = fmt("#%u ! %s", d.addr, f.size() >= 5 ? exception_name(f[2]) : "exception");
        pending_.erase(d.addr);
        return d;
    }

    const auto pend = pending_.find(d.addr);
    const bool have_req = pend != pending_.end() && pend->second.fc == fc;

    if (is_read(fc)) {
        const bool fits_response = f.size() == static_cast<size_t>(f[2]) + 5;
        const bool fits_request = f.size() == 8;
        // a response needs a request before it, unless only a response fits
        if (fits_response && (have_req || !fits_request)) {
            d.kind = Description::Kind::Response;
            if (!is_bit_fn(fc)) d.regs = regs_from(f, 3, f[2]);
            if (have_req) {
                d.start = pend->second.start;
                d.start_known = true;
                pending_.erase(pend);
            }
            if (is_bit_fn(fc)) {
                d.text = fmt("#%u -> %u bytes of bits", d.addr, f[2]);
            } else if (d.start_known) {
                d.text = fmt("#%u -> %zu regs @%u", d.addr, d.regs.size(), d.start);
            } else {
                d.text = fmt("#%u -> %zu regs", d.addr, d.regs.size());
            }
            return d;
        }
        if (fits_request) {
            Request r;
            r.addr = d.addr;
            r.fc = fc;
            r.start = be16(f, 2);
            r.count = be16(f, 4);
            pending_[d.addr] = r;
            d.kind = Description::Kind::Request;
            d.start = r.start;
            d.start_known = true;
            d.text = fmt("#%u %s %u x%u", d.addr, name, r.start, r.count);
            return d;
        }
        d.text = fmt("#%u %s ?", d.addr, name);
        return d;
    }

    if ((fc == kWriteCoil || fc == kWriteRegister) && f.size() == 8) {
        // request and reply are identical
        const uint16_t a = be16(f, 2), v = be16(f, 4);
        const bool echo = have_req && pend->second.start == a;
        d.kind = echo ? Description::Kind::Response : Description::Kind::Request;
        if (echo) pending_.erase(pend);
        else pending_[d.addr] = Request{d.addr, fc, a, 1, {v}};
        d.start = a;
        d.start_known = true;
        if (fc == kWriteCoil) d.text = fmt("#%u %s %u = %s%s", d.addr, name, a, v ? "ON" : "OFF", echo ? " ok" : "");
        else d.text = fmt("#%u %s %u = %u%s", d.addr, name, a, v, echo ? " ok" : "");
        return d;
    }
    if (fc == kWriteCoils || fc == kWriteRegisters) {
        const uint16_t a = be16(f, 2), n = be16(f, 4);
        if (f.size() == 8) {
            d.kind = Description::Kind::Response;
            pending_.erase(d.addr);
            d.text = fmt("#%u %s %u x%u ok", d.addr, name, a, n);
        } else {
            d.kind = Description::Kind::Request;
            pending_[d.addr] = Request{d.addr, fc, a, n, {}};
            if (fc == kWriteRegisters && f.size() >= 9) d.regs = regs_from(f, 7, f[6]);
            d.text = fmt("#%u %s %u x%u", d.addr, name, a, n);
        }
        d.start = a;
        d.start_known = true;
        return d;
    }
    d.text = fmt("#%u function %u", d.addr, fc);
    return d;
}

std::vector<Bytes> split_frames(const Bytes& data) {
    if (data.size() < 8 || crc_ok(data)) return {data};
    // greedy: shortest prefix with a valid CRC, then the rest recursively
    for (size_t cut = 4; cut + 4 <= data.size(); ++cut) {
        Bytes head(data.begin(), data.begin() + static_cast<long>(cut));
        if (!crc_ok(head)) continue;
        Bytes tail(data.begin() + static_cast<long>(cut), data.end());
        auto rest = split_frames(tail);
        if (rest.size() == 1 && !crc_ok(rest[0])) continue;
        rest.insert(rest.begin(), std::move(head));
        return rest;
    }
    return {data};
}

// ------------------------------------------------------------ master

Bytes Master::start(const Request& r, uint64_t now_us) {
    req_ = r;
    sent_ = encode(r);
    partial_.clear();
    busy_ = true;
    sent_us_ = now_us;
    ++stats_.sent;
    return sent_;
}

Event Master::on_frame(const Bytes& frame, uint64_t now_us) {
    if (!busy_) return Event::None;
    if (frame == sent_) return Event::None;  // our own request echoed (RS485 without echo suppression)

    // a pause inside a slow device's reply splits it; glue pieces until the CRC fits
    Bytes data = partial_;
    data.insert(data.end(), frame.begin(), frame.end());
    if (!crc_ok(data) && data.size() < 256) {
        const bool may_grow = is_read(req_.fc) && data.size() >= 3 && data[1] == req_.fc &&
                              data.size() < static_cast<size_t>(data[2]) + 5;
        if (may_grow || data.size() < 5) {
            partial_ = data;
            return Event::None;
        }
    }
    partial_.clear();
    last_ = decode_response(req_, data);
    busy_ = false;
    stats_.last_ms = static_cast<uint32_t>((now_us - sent_us_) / 1000);
    if (last_.ok) {
        ++stats_.ok;
        return Event::Ok;
    }
    if (last_.exception) {
        ++stats_.exceptions;
        return Event::Exception;
    }
    ++stats_.errors;
    return Event::Error;
}

Event Master::on_tick(uint64_t now_us) {
    if (!busy_) return Event::None;
    if (now_us - sent_us_ < static_cast<uint64_t>(timeout_ms) * 1000) return Event::None;
    busy_ = false;
    last_ = Response{};
    last_.error = partial_.empty() ? "no response" : "incomplete response";
    partial_.clear();
    ++stats_.timeouts;
    return Event::Timeout;
}

// ------------------------------------------------------------ scanner

void Scanner::start(const Request& probe, uint8_t first, uint8_t last) {
    probe_ = probe;
    first_ = first < 1 ? 1 : first;
    last_ = last > 247 ? 247 : last;
    if (last_ < first_) last_ = first_;
    addr_ = first_;
    found_.clear();
    running_ = true;
}

Request Scanner::probe() const {
    Request r = probe_;
    r.addr = addr_;
    return r;
}

bool Scanner::advance(Event ev, const Response& r) {
    if (!running_) return false;
    if (ev == Event::Ok || ev == Event::Exception) found_.push_back({addr_, r.exception, r.code});
    if (addr_ >= last_) {
        running_ = false;
        return false;
    }
    ++addr_;
    return true;
}

// ------------------------------------------------------------ registers

const char* interp_name(Interp i) {
    switch (i) {
        case Interp::U16: return "U16";
        case Interp::I16: return "I16";
        case Interp::Hex: return "HEX";
        case Interp::U32: return "U32 ABCD";
        case Interp::U32Swap: return "U32 CDAB";
        case Interp::I32: return "I32 ABCD";
        case Interp::F32: return "F32 ABCD";
        case Interp::F32Swap: return "F32 CDAB";
    }
    return "";
}

Interp next_interp(Interp i, int dir) {
    const int n = 8;
    return static_cast<Interp>((static_cast<int>(i) + dir + n) % n);
}

bool is_pair(Interp i) { return i == Interp::U32 || i == Interp::U32Swap || i == Interp::I32 || i == Interp::F32 || i == Interp::F32Swap; }

std::string format_value(const std::vector<uint16_t>& regs, size_t i, Interp interp) {
    if (i >= regs.size()) return "";
    const uint16_t a = regs[i];
    char buf[40];
    switch (interp) {
        case Interp::U16:
            std::snprintf(buf, sizeof buf, "%u", a);
            return buf;
        case Interp::I16:
            std::snprintf(buf, sizeof buf, "%d", static_cast<int16_t>(a));
            return buf;
        case Interp::Hex:
            std::snprintf(buf, sizeof buf, "0x%04X", a);
            return buf;
        default:
            break;
    }
    if (i + 1 >= regs.size()) return "";
    const uint16_t b = regs[i + 1];
    const bool swap = interp == Interp::U32Swap || interp == Interp::F32Swap;
    const uint32_t v = swap ? (static_cast<uint32_t>(b) << 16 | a) : (static_cast<uint32_t>(a) << 16 | b);
    switch (interp) {
        case Interp::U32:
        case Interp::U32Swap:
            std::snprintf(buf, sizeof buf, "%u", v);
            break;
        case Interp::I32:
            std::snprintf(buf, sizeof buf, "%d", static_cast<int32_t>(v));
            break;
        default: {
            float f;
            std::memcpy(&f, &v, sizeof f);
            std::snprintf(buf, sizeof buf, "%.6g", static_cast<double>(f));
            break;
        }
    }
    return buf;
}

}  // namespace ser::modbus
