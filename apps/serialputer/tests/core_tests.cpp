// Host tests for the SerialPuter core. Non-zero exit on failure.
#include "capture.h"
#include "modbus.h"
#include "serial_format.h"
#include "session.h"
#include "sim_bus.h"

#include <cstdio>
#include <string>

using namespace ser;
namespace mb = ser::modbus;

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool ok, const std::string& what) {
    ++g_checks;
    if (!ok) {
        ++g_failures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

template <class A, class B>
void eq(const A& got, const B& want, const std::string& what) {
    check(got == want, what);
}

void eqs(const std::string& got, const std::string& want, const std::string& what) {
    check(got == want, what + ": got '" + got + "', want '" + want + "'");
}

Bytes B(std::initializer_list<int> v) {
    Bytes b;
    for (int x : v) b.push_back(static_cast<uint8_t>(x));
    return b;
}

void test_format() {
    SerialFormat f;
    eq(char_bits(f), 10, "8N1 bits");
    eq(char_time_us(f), 1042u, "9600 char time");
    eq(modbus_gap_us(f), 3646u, "9600 modbus gap");
    f.baud = 115200;
    eq(modbus_gap_us(f), 1750u, "fast modbus gap");
    check(parse_frame("8e1", f), "parse 8e1");
    eq(char_bits(f), 11, "8E1 bits");
    eqs(frame_name(f), "8E1", "frame name");
    check(!parse_frame("9N1", f), "reject 9N1");
    check(!parse_frame("8X1", f), "reject 8X1");
    eq(next_baud(9600, 1), 19200, "next baud");
    eq(next_baud(1200, -1), 921600, "baud wraps");
    eq(next_baud(10000, 1), 19200, "odd baud up");
    eq(next_baud(10000, -1), 9600, "odd baud down");
    eqs(next_frame("8N1", 1), "8E1", "next frame");
    eqs(next_frame("weird", 1), "8E1", "unknown frame -> from start");
}

void test_framer() {
    Framer fr;
    FramerConfig c;
    c.gap_us = 4000;
    c.char_us = 1000;
    fr.configure(c);
    std::vector<Frame> out;

    const Bytes a = B({1, 3, 0, 0});
    fr.feed(a.data(), a.size(), 10000, out);
    const Bytes b = B({0, 10});
    fr.feed(b.data(), b.size(), 12000, out);  // continues: first byte right after
    eq(out.size(), size_t{0}, "no frame yet");
    fr.idle(14000, out);
    eq(out.size(), size_t{0}, "gap not reached");
    fr.idle(16500, out);
    eq(out.size(), size_t{1}, "frame on idle");
    eq(out[0].bytes.size(), size_t{6}, "frame bytes");
    eq(out[0].t_us, uint64_t{6000}, "frame start estimated");

    out.clear();
    const Bytes x = B({0xAA});
    fr.feed(x.data(), 1, 100000, out);
    fr.feed(x.data(), 1, 110000, out);  // 10 ms later: new frame
    eq(out.size(), size_t{1}, "gap closes a frame on feed");
    fr.flush(out);
    eq(out.size(), size_t{2}, "flush");

    // line mode
    c.split = Split::Line;
    fr.configure(c);
    out.clear();
    const std::string t = "OK\r\nERR";
    fr.feed(reinterpret_cast<const uint8_t*>(t.data()), t.size(), 200000, out);
    eq(out.size(), size_t{1}, "line closed on LF");
    eqs(text_string(out[0].bytes), "OK\\r\\n", "line text");
    fr.idle(260000, out);
    eq(out.size(), size_t{1}, "prompt waits for idle");
    fr.idle(400000, out);
    eq(out.size(), size_t{2}, "prompt flushed");
    eqs(text_string(out[1].bytes), "ERR", "prompt text");

    // max frame
    c.split = Split::Gap;
    c.max_frame = 4;
    fr.configure(c);
    out.clear();
    const Bytes big(10, 0x55);
    fr.feed(big.data(), big.size(), 500000, out);
    eq(out.size(), size_t{2}, "max frame splits");
}

void test_capture_views() {
    Capture cap(3);
    for (int i = 0; i < 5; ++i) cap.add(Frame{i % 2 ? Dir::Tx : Dir::Rx, uint64_t(1000 + i), B({i})});
    eq(cap.frames().size(), size_t{3}, "capture keeps last");
    eq(cap.rx_bytes(), uint64_t{3}, "rx count");
    eq(cap.tx_bytes(), uint64_t{2}, "tx count");
    eq(cap.start_us(), uint64_t{1000}, "start time");

    Frame f{Dir::Rx, 0, B({1, 3, 0x14, 'A', 'B', '\r', '\n', 0xFF})};
    auto lines = wrap_frame(f, View::Hex, 12);  // 4 bytes per line
    eq(lines.size(), size_t{2}, "hex wraps");
    eqs(lines[0], "01 03 14 41", "hex line");
    lines = wrap_frame(f, View::Text, 40);
    eqs(lines[0], "\\x01\\x03\\x14AB\\r\\n\\xFF", "text escapes");
    lines = wrap_frame(f, View::Text, 8);
    eqs(lines[0], "\\x01\\x03", "escapes not split");
    lines = wrap_frame(f, View::Dump, 21);  // 5 per line
    eqs(lines[0], "01 03 14 41 42  ...AB", "dump line");
    eqs(lines[1], "0D 0A FF        ...", "dump tail padded");

    eqs(time_label(TimeMode::Rel, 3141500, 0, 0), "3.141", "rel time");
    eqs(time_label(TimeMode::Delta, 112000, 0, 100000), "+12ms", "delta ms");
    eqs(time_label(TimeMode::Delta, 20000000, 0, 0), "+20.0s", "delta s");
    eqs(time_label(TimeMode::Off, 5, 0, 0), "", "time off");
    eqs(log_line(Frame{Dir::Tx, 0, B({'A', '\n'})}, "t"), "t TX 41 0A  |A\\n|", "log line");
}

void test_modbus_codec() {
    mb::Request r;
    r.addr = 1;
    r.fc = mb::kReadHolding;
    r.start = 0;
    r.count = 10;
    eqs(hex_string(mb::encode(r)), "01 03 00 00 00 0A C5 CD", "read holding request");
    check(mb::crc_ok(B({0x01, 0x03, 0x00, 0x00, 0x00, 0x0A, 0xC5, 0xCD})), "crc ok");
    check(!mb::crc_ok(B({0x01, 0x03, 0x00, 0x00, 0x00, 0x0A, 0xC5, 0xCE})), "crc bad");

    mb::Request w{0x11, mb::kWriteRegister, 1, 1, {3}};
    eqs(hex_string(mb::encode(w)), "11 06 00 01 00 03 9A 9B", "write register (spec example)");
    mb::Request ww{1, mb::kWriteRegisters, 100, 2, {0x000A, 0x0102}};
    const auto wb = mb::encode(ww);
    eqs(hex_string(wb, 0, 11), "01 10 00 64 00 02 04 00 0A 01 02", "write registers body");

    // responses
    r.count = 2;
    Bytes resp = B({1, 3, 4, 0x14, 0xC0, 0xFF, 0x83});
    mb::append_crc(resp);
    auto d = mb::decode_response(r, resp);
    check(d.ok, "decode ok");
    eq(d.regs.size(), size_t{2}, "two regs");
    eq(d.regs[0], uint16_t{0x14C0}, "reg0");

    Bytes ex = B({1, 0x83, 2});
    mb::append_crc(ex);
    d = mb::decode_response(r, ex);
    check(!d.ok && d.exception && d.code == 2, "exception");
    eqs(d.error, "ILLEGAL DATA ADDRESS", "exception text");

    Bytes wrong = B({2, 3, 4, 0, 0, 0, 0});
    mb::append_crc(wrong);
    d = mb::decode_response(r, wrong);
    check(!d.ok && d.error.find("#2") != std::string::npos, "wrong address");
    resp.back() ^= 1;
    d = mb::decode_response(r, resp);
    eqs(d.error, "CRC error", "crc error");

    mb::Request coils{1, mb::kReadCoils, 0, 10, {}};
    Bytes cr = B({1, 1, 2, 0b00100101, 0b10});
    mb::append_crc(cr);
    d = mb::decode_response(coils, cr);
    check(d.ok && d.bits.size() == 10, "coils decoded");
    check(d.bits[0] && !d.bits[1] && d.bits[2] && d.bits[5] && d.bits[9] && !d.bits[8], "coil values");

    eq(mb::next_read_fn(4, 1), uint8_t{1}, "fn wraps up");
    eq(mb::next_read_fn(1, -1), uint8_t{4}, "fn wraps down");
}

void test_sniffer() {
    mb::Sniffer s;
    const Bytes req = mb::encode({1, 3, 6, 4, {}});
    auto d = s.describe(req);
    check(d.kind == mb::Description::Kind::Request, "request kind");
    eqs(d.text, "#1 READ HOLDING 6 x4", "request text");

    Bytes resp = B({1, 3, 8, 0x0C, 0xF6, 0x0C, 0xF9, 0x0C, 0xF8, 0x0C, 0xF7});
    mb::append_crc(resp);
    d = s.describe(resp);
    check(d.kind == mb::Description::Kind::Response, "response kind");
    check(d.start_known && d.start == 6, "response start from request");
    eq(d.regs.size(), size_t{4}, "response regs");
    eqs(d.text, "#1 -> 4 regs @6", "response text");

    // 3-byte coil response is 8 bytes long like a request: only with a pending request
    const Bytes creq = mb::encode({2, 1, 0, 20, {}});
    s.describe(creq);
    Bytes cresp = B({2, 1, 3, 1, 2, 3});
    mb::append_crc(cresp);
    d = s.describe(cresp);
    check(d.kind == mb::Description::Kind::Response, "8-byte coil response");

    Bytes ex = B({5, 0x84, 1});
    mb::append_crc(ex);
    d = s.describe(ex);
    check(d.kind == mb::Description::Kind::Exception, "exception kind");
    eqs(d.text, "#5 ! ILLEGAL FUNCTION", "exception text");

    const Bytes wr = mb::encode({1, 6, 100, 1, {7}});
    d = s.describe(wr);
    eqs(d.text, "#1 WRITE REG 100 = 7", "write request");
    d = s.describe(wr);
    eqs(d.text, "#1 WRITE REG 100 = 7 ok", "write echo");

    d = s.describe(B({'O', 'K', '\r', '\n'}));
    check(!d.crc_ok, "text is not modbus");

    // two frames glued together
    Bytes glued = req;
    glued.insert(glued.end(), resp.begin(), resp.end());
    const auto parts = mb::split_frames(glued);
    eq(parts.size(), size_t{2}, "split glued frames");
    check(parts[0] == req && parts[1] == resp, "split parts");
    eq(mb::split_frames(B({1, 2, 3, 4, 5, 6, 7, 8, 9})).size(), size_t{1}, "no split for garbage");
}

void test_master() {
    SimBus bus;
    mb::Master m;
    m.timeout_ms = 100;
    mb::Request r{1, mb::kReadHolding, 0, 6, {}};
    const Bytes sent = m.start(r, 0);
    check(m.busy(), "busy after start");
    eq(m.on_frame(sent, 1000), mb::Event::None, "echo ignored");
    const Bytes reply = bus.respond(sent, 0);
    // arrives in two pieces
    const Bytes a(reply.begin(), reply.begin() + 5), b(reply.begin() + 5, reply.end());
    eq(m.on_frame(a, 5000), mb::Event::None, "partial kept");
    eq(m.on_frame(b, 9000), mb::Event::Ok, "glued response ok");
    eq(m.last().regs.size(), size_t{6}, "six regs");
    eq(m.last().regs[2], uint16_t{87}, "SOC from sim");
    eq(m.stats().last_ms, 9u, "response time");

    m.start({1, mb::kReadHolding, 30, 20, {}}, 20000);
    eq(m.on_frame(bus.respond(mb::encode({1, 3, 30, 20, {}}), 0), 25000), mb::Event::Exception, "exception event");

    m.start({99, mb::kReadHolding, 0, 1, {}}, 100000);
    eq(m.on_tick(150000), mb::Event::None, "before timeout");
    eq(m.on_tick(201000), mb::Event::Timeout, "timeout");
    eqs(m.last().error, "no response", "timeout text");
    eq(m.stats().sent, 3u, "sent count");
    eq(m.stats().ok, 1u, "ok count");
    eq(m.stats().exceptions, 1u, "exception count");
    eq(m.stats().timeouts, 1u, "timeout count");
}

void test_scanner() {
    SimBus bus;
    mb::Scanner sc;
    sc.start({0, mb::kReadHolding, 0, 1, {}}, 1, 20);
    int steps = 0;
    while (sc.running()) {
        mb::Master m;
        const Bytes q = m.start(sc.probe(), 0);
        const Bytes a = bus.respond(q, 0);
        const mb::Event ev = a.empty() ? m.on_tick(10000000) : m.on_frame(a, 1000);
        sc.advance(ev, m.last());
        ++steps;
    }
    eq(steps, 20, "scanned 20 addresses");
    eq(sc.found().size(), size_t{2}, "found BMS and inverter");
    if (sc.found().size() == 2) {
        eq(sc.found()[0].addr, uint8_t{1}, "bms address");
        eq(sc.found()[1].addr, uint8_t{16}, "inverter address");
    }
}

void test_values() {
    const std::vector<uint16_t> regs = {0x14C0, 0xFF83, 0x0000, 0x439D};
    eqs(mb::format_value(regs, 0, mb::Interp::U16), "5312", "u16");
    eqs(mb::format_value(regs, 1, mb::Interp::I16), "-125", "i16");
    eqs(mb::format_value(regs, 1, mb::Interp::Hex), "0xFF83", "hex");
    eqs(mb::format_value(regs, 2, mb::Interp::F32Swap), "314", "f32 cdab");
    eqs(mb::format_value(regs, 3, mb::Interp::F32), "", "pair incomplete");
    eqs(mb::format_value({0x439D, 0x0000}, 0, mb::Interp::F32), "314", "f32 abcd");
    eqs(mb::format_value({0x0001, 0x0002}, 0, mb::Interp::U32), "65538", "u32");
    eqs(mb::format_value({0x0001, 0x0002}, 0, mb::Interp::U32Swap), "131073", "u32 swap");
    eqs(mb::format_value({0xFFFF, 0xFFFE}, 0, mb::Interp::I32), "-2", "i32");
    check(mb::next_interp(mb::Interp::F32Swap) == mb::Interp::U16, "interp wraps");
}

void test_sim_text() {
    SimBus bus;
    const std::string at = "AT\r";
    const Bytes r = bus.respond(Bytes(at.begin(), at.end()), 0);
    eqs(std::string(r.begin(), r.end()), "OK\r\n", "AT -> OK");
    const std::string part1 = "at+c", part2 = "sq\r\n";
    check(bus.respond(Bytes(part1.begin(), part1.end()), 0).empty(), "partial command");
    const Bytes r2 = bus.respond(Bytes(part2.begin(), part2.end()), 0);
    eqs(std::string(r2.begin(), r2.end()), "+CSQ: 21,0\r\nOK\r\n", "split command");
    check(bus.respond(mb::encode({7, 3, 0, 1, {}}), 0).empty(), "absent device is silent");
}

void test_send() {
    auto r = prepare_send("AT", SendMode::Text, LineEnd::CRLF, false);
    check(r.ok && hex_string(r.bytes) == "41 54 0D 0A", "text + CRLF");
    r = prepare_send("a\\x01\\n", SendMode::Text, LineEnd::None, false);
    check(r.ok && hex_string(r.bytes) == "61 01 0A", "text escapes");
    r = prepare_send("\\xZ1", SendMode::Text, LineEnd::None, false);
    check(!r.ok, "bad escape");
    r = prepare_send("01 03 00 00 00 0A", SendMode::Hex, LineEnd::CRLF, true);
    check(r.ok && hex_string(r.bytes) == "01 03 00 00 00 0A C5 CD", "hex + crc, no line end");
    r = prepare_send("01 zz", SendMode::Hex, LineEnd::None, false);
    check(!r.ok, "bad hex fails");
    r = prepare_send("", SendMode::Text, LineEnd::None, false);
    check(!r.ok, "empty");
}

void test_session() {
    Session s;
    s.port = "/dev/ttyAMA0";
    s.fmt.baud = 19200;
    parse_frame("8E1", s.fmt);
    s.split = Split::Line;
    s.gap_ms = 7;
    s.view = View::Dump;
    s.time = TimeMode::Delta;
    s.send = SendMode::Hex;
    s.line_end = LineEnd::LF;
    s.append_crc = true;
    s.modbus_labels = false;
    s.req = {16, mb::kReadInput, 30, 12, {}};
    s.interp = mb::Interp::F32Swap;
    s.poll_ms = 500;
    s.timeout_ms = 200;
    s.scan_first = 10;
    s.scan_last = 20;
    s.remember("AT");
    s.remember("AT");
    s.remember("01 03\t00");

    Session t;
    t.deserialize(s.serialize());
    eqs(t.port, "/dev/ttyAMA0", "port");
    check(t.fmt == s.fmt, "format");
    check(t.split == Split::Line && t.gap_ms == 7, "split");
    check(t.view == View::Dump && t.time == TimeMode::Delta, "view/time");
    check(t.send == SendMode::Hex && t.line_end == LineEnd::LF && t.append_crc, "send");
    check(!t.modbus_labels, "labels");
    check(t.req.addr == 16 && t.req.fc == 4 && t.req.start == 30 && t.req.count == 12, "request");
    check(t.interp == mb::Interp::F32Swap, "interp");
    check(t.poll_ms == 500 && t.timeout_ms == 200, "poll");
    check(t.scan_first == 10 && t.scan_last == 20, "scan");
    eq(t.history.size(), size_t{2}, "history dedup");
    if (t.history.size() == 2) eqs(t.history[1], "01 03\t00", "history keeps tabs");

    Session u;
    u.deserialize("garbage\nport\tX\n");
    eqs(u.port, kSimPort, "bad magic ignored");

    Session g;
    g.fmt.baud = 9600;
    eq(g.framer().gap_us, 3646u, "auto gap at 9600");
    g.fmt.baud = 115200;
    eq(g.framer().gap_us, 2000u, "auto gap floor");
    g.gap_ms = 20;
    eq(g.framer().gap_us, 20000u, "fixed gap");
}

}  // namespace

int main() {
    test_format();
    test_framer();
    test_capture_views();
    test_modbus_codec();
    test_sniffer();
    test_master();
    test_scanner();
    test_values();
    test_sim_text();
    test_send();
    test_session();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
