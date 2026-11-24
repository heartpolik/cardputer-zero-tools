// Host tests for the BitPuter core. Non-zero exit on failure.
#include "checksum.h"
#include "decode.h"
#include "int_expr.h"
#include "session.h"
#include "word.h"

#include <cstdio>
#include <string>

using namespace bits;

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

Env make_env(int width = 32, bool is_signed = false, Base input = Base::Dec) {
    Env e;
    e.word = {width, is_signed};
    e.input = input;
    return e;
}

void eq(const std::string& src, uint64_t expected, const Env& env = make_env()) {
    const auto r = evaluate(src, env);
    if (!r.ok) {
        check(false, src + " -> error: " + r.error);
        return;
    }
    check(r.value == expected, src + " = " + std::to_string(r.value) + ", expected " + std::to_string(expected));
}

void err(const std::string& src, const std::string& part, const Env& env = make_env(), size_t pos = std::string::npos) {
    const auto r = evaluate(src, env);
    check(!r.ok, src + " should fail");
    if (r.ok) return;
    check(r.error.find(part) != std::string::npos, src + " error '" + r.error + "' lacks '" + part + "'");
    if (pos != std::string::npos) {
        check(r.error_pos == pos, src + " error at " + std::to_string(r.error_pos) + ", expected " + std::to_string(pos));
    }
}

void test_literals() {
    eq("255", 255);
    eq("0xFF", 255);
    eq("0o17", 15);
    eq("0b1010", 10);
    eq("0n99", 99);
    eq("1_000_000", 1000000);
    eq("4K", 4096);
    eq("2M", 2u << 20);
    eq("1g", 1u << 30);
    eq("'A'", 65);
    eq("'AB'", 0x4142);
    eq("'\\n'", 10);
    eq("f32(1.0)", 0x3F800000);
    eq("f32(-2.5)", 0xC0200000);
    eq("f64(1.0)", 0x3FF0000000000000ULL, make_env(64));
    const auto hex = make_env(32, false, Base::Hex);
    eq("ff", 255, hex);
    eq("FF + 1", 256, hex);
    eq("cafe", 0xCAFE, hex);
    eq("0b1", 0xB1, hex);  // 0b is hex in HEX mode
    eq("0n10", 10, hex);
    eq("10", 16, hex);
    eq("101", 5, make_env(32, false, Base::Bin));
    eq("17", 15, make_env(32, false, Base::Oct));
    err("12", "binary digit", make_env(32, false, Base::Bin), 1);
    err("0xZZ", "not a hex digit");
    err("99999999999999999999", "too large");
    err("'abcdefghi'", "more than 8");
}

void test_wrapping() {
    eq("0xFF + 1", 0, make_env(8));
    eq("-1", 0xFF, make_env(8));
    eq("-1", 0xFFFFFFFF);
    eq("-1", ~uint64_t{0}, make_env(64));
    eq("0x1FF", 0xFF, make_env(8));
    eq("200 * 2", 144, make_env(8));
    eq("~0", 0xFFFF, make_env(16));
    eq("2 ** 10", 1024);
    eq("2 ** 32", 0);
    eq("3 ** 2 ** 2", 81);
    eq("-2 ** 2", 0xFFFFFFFC);  // -(2**2)
}

void test_operators() {
    eq("1 + 2 * 3", 7);
    eq("(1 + 2) * 3", 9);
    eq("0xF0 | 0x0F", 0xFF);
    eq("0xFF & 0x0F", 0x0F);
    eq("0xFF ^ 0x0F", 0xF0);
    eq("1 << 4", 16);
    eq("1 << 32", 0);
    eq("0x80000000 >> 31", 1);
    eq("1 | 2 ^ 3 & 4", 1 | (2 ^ (3 & 4)));
    eq("1 + 1 << 2", 8);  // shifts bind looser than +
    eq("7 / 2", 3);
    eq("7 % 4", 3);
    err("1 / 0", "division by zero", make_env(), 2);
    err("1 % (2 - 2)", "division by zero");
    err("(1 + 2", "missing ')'");
    err("1 + 2)", "unmatched ')'");
    err("1 +", "missing value");
    err("foo", "unknown name");
}

void test_signed() {
    const auto s8 = make_env(8, true);
    eq("-128 / -1", 0x80, s8);  // wraps like the hardware
    eq("-7 / 2", static_cast<uint8_t>(-3), s8);
    eq("-7 % 2", static_cast<uint8_t>(-1), s8);
    eq("0x80 >> 1", 0xC0, s8);    // arithmetic
    eq("0x80 >>> 1", 0x40, s8);   // logical
    eq("0x80 >> 7", 0xFF, s8);
    eq("0x80 >> 9", 0xFF, s8);
    eq("0x80 >> 1", 0x40, make_env(8));
    eq("min(-1, 1)", 0xFF, s8);
    eq("min(-1, 1)", 1, make_env(8));
    eq("max(-1, 1)", 1, s8);
    eq("abs(-5)", 5, s8);
    Word w{8, true};
    check(w.to_signed(0xFF) == -1, "to_signed 0xFF");
    check(w.to_signed(0x7F) == 127, "to_signed 0x7F");
    check(Word{64, true}.to_signed(~uint64_t{0}) == -1, "to_signed 64");
}

void test_functions() {
    eq("popcnt(0xFF)", 8);
    eq("parity(7)", 1);
    eq("clz(1)", 31);
    eq("clz(1)", 7, make_env(8));
    eq("clz(0)", 16, make_env(16));
    eq("ctz(8)", 3);
    eq("ctz(0)", 32);
    eq("bswap(0x12345678)", 0x78563412);
    eq("bswap(0x1234)", 0x3412, make_env(16));
    eq("rev(1)", 0x80, make_env(8));
    eq("bit(7)", 128);
    eq("mask(4)", 15);
    eq("mask(7, 4)", 0xF0);
    eq("mask(32)", 0xFFFFFFFF);
    eq("field(0xABCD, 11, 8)", 0xB);
    eq("set(0, 3)", 8);
    eq("clr(0xFF, 0)", 0xFE);
    eq("tgl(5, 0)", 4);
    eq("rol(0x81, 1)", 0x03, make_env(8));
    eq("ror(0x81, 1)", 0xC0, make_env(8));
    eq("rol(0x12345678, 8)", 0x34567812);
    eq("rol(5, 32)", 5);
    eq("sext(0x80, 8)", 0xFFFFFF80);
    eq("sext(0x7F, 8)", 0x7F);
    eq("zext(0xFFFF, 4)", 0xF);
    eq("i16(0xFFFE)", 0xFFFFFFFE);
    eq("u8(0x1234)", 0x34);
    eq("align(13, 8)", 16);
    eq("align(16, 8)", 16);
    eq("isqrt(99)", 9);
    eq("isqrt(0xFFFFFFFFFFFFFFFF)", 0xFFFFFFFF, make_env(64));
    eq("log2(1024)", 10);
    eq("gcd(12, 18)", 6);
    err("bit(32)", "outside the 32-bit word");
    err("mask(3, 5)", "hi >= lo");
    err("popcnt(1, 2)", "takes 1 argument");
    err("nope(1)", "unknown function");
    err("popcnt", "needs (...)");
    err("align(5, 0)", "align to 0");
    err("log2(0)", "log2(0)");
    err("f32(abc)", "expected a number");
}

void test_variables() {
    Env env = make_env();
    auto r = submit("x = 0x10", env);
    check(r.ok && r.assigned == "x" && env.vars["x"] == 16, "assign x");
    check(env.vars["ans"] == 16, "assignment sets ans");
    r = submit("x * 2", env);
    check(r.ok && r.value == 32 && env.vars["ans"] == 32, "use x");
    eq("ans + 1", 1);  // fresh env: ans = 0
    r = evaluate("y = 1", env);
    check(r.ok && r.assigned == "y" && env.vars.count("y") == 0, "evaluate does not assign");
    err("popcnt = 1", "is a function");
    err("ff = 1", "hex number in HEX mode", make_env(32, false, Base::Hex));
    Env hex = make_env(32, false, Base::Hex);
    r = submit("val = 10", hex);
    check(r.ok && hex.vars["val"] == 16, "hex-mode variable");
}

void test_format() {
    check(format_hex(0xFF, 8) == "0xFF", "hex 8");
    check(format_hex(0x1234ABCD, 32) == "0x1234_ABCD", "hex 32");
    check(format_hex(0x1, 64) == "0x0000_0000_0000_0001", "hex 64");
    check(format_bin(5, 8) == "0000 0101", "bin 8");
    check(format_oct(8) == "0o10", "oct");
    check(format_oct(0) == "0o0", "oct 0");
    check(format_dec(0xFF, Word{8, true}, true) == "-1", "dec signed");
    check(format_dec(0xFF, Word{8, true}, false) == "255", "dec unsigned");
    check(literal(255, Word{32, false}, Base::Hex) == "FF", "literal hex");
    check(literal(5, Word{32, false}, Base::Bin) == "101", "literal bin");
    check(literal(0xFFFFFFFF, Word{32, true}, Base::Dec) == "-1", "literal signed dec");
    // round trip through the parser in every base
    for (Base b : {Base::Dec, Base::Hex, Base::Bin, Base::Oct}) {
        for (bool s : {false, true}) {
            Env env = make_env(16, s, b);
            for (uint64_t v : {0ULL, 1ULL, 0x7FFFULL, 0x8000ULL, 0xFFFFULL, 0x1234ULL}) {
                const auto r = evaluate(literal(v, env.word, b), env);
                check(r.ok && r.value == v, std::string("round trip ") + base_name(b) + " " + std::to_string(v));
            }
        }
    }
}

void test_decode() {
    check(decode_chars(0x41424344, 32) == "'ABCD'", "chars");
    check(decode_chars(0x4100, 16) == "'A.'", "chars nonprint");
    check(decode_float(0x3FC00000, 32) == "f32 1.5", "float32 " + decode_float(0x3FC00000, 32));
    check(decode_float(0x3FF8000000000000ULL, 64) == "f64 1.5", "float64");
    check(decode_float(1, 16).find("32 or 64") != std::string::npos, "float needs width");
    check(decode_time(0, Word{32, false}) == "1970-01-01 00:00:00", "epoch");
    check(decode_time(1764000000, Word{32, false}) == "2025-11-24 16:00:00", "time " + decode_time(1764000000, Word{32, false}));
    check(decode_time(1764000000123ULL, Word{64, false}) == "2025-11-24 16:00:00.123 (ms)", "time ms");
    check(decode_time(0xFFFFFFFF, Word{32, true}) == "1969-12-31 23:59:59", "negative time");
}

void test_checksums() {
    const std::string check_str = "123456789";
    const std::vector<uint8_t> data(check_str.begin(), check_str.end());
    auto find = [](const std::string& name) -> const CrcSpec& {
        for (const auto& s : checksums()) {
            if (name == s.name) return s;
        }
        std::printf("no spec %s\n", name.c_str());
        return checksums()[0];
    };
    // standard "123456789" check values (reveng catalogue)
    const std::pair<const char*, uint32_t> expected[] = {
        {"CRC-16/MODBUS", 0x4B37}, {"CRC-32", 0xCBF43926},   {"CRC-32C", 0xE3069283},
        {"CRC-16/CCITT", 0x29B1},  {"CRC-16/XMODEM", 0x31C3}, {"CRC-16/KERMIT", 0x2189},
        {"CRC-16/ARC", 0xBB3D},    {"CRC-8", 0xF4},           {"CRC-8/MAXIM", 0xA1},
        {"CRC-8/NRSC-5", 0xF7},    {"XOR8", 0x31},            {"SUM8", 0xDD},
    };
    for (const auto& [name, value] : expected) {
        const uint32_t got = compute(find(name), data);
        char buf[96];
        std::snprintf(buf, sizeof buf, "%s check = 0x%X, expected 0x%X", name, got, value);
        check(got == value, buf);
    }
    check(wire_bytes(find("CRC-16/MODBUS"), 0x4B37) == "37 4B", "modbus wire order");
    check(wire_bytes(find("CRC-32"), 0xCBF43926) == "CB F4 39 26", "crc32 wire order");
    // Modbus frame "01 03 00 00 00 0A" -> CRC C5 CD on the wire
    const auto frame = parse_bytes("01 03 00 00 00 0A");
    check(frame.ok && frame.bytes.size() == 6, "parse frame");
    check(wire_bytes(find("CRC-16/MODBUS"), compute(find("CRC-16/MODBUS"), frame.bytes)) == "C5 CD", "modbus frame crc");
}

void test_parse_bytes() {
    auto r = parse_bytes("0x01,0x02 0A:0b");
    check(r.ok && r.bytes == std::vector<uint8_t>({1, 2, 0x0A, 0x0B}), "mixed separators");
    r = parse_bytes("010203");
    check(r.ok && r.bytes.size() == 3, "packed hex");
    r = parse_bytes("1 2");
    check(r.ok && r.bytes == std::vector<uint8_t>({1, 2}), "single digits");
    r = parse_bytes("123");
    check(!r.ok && r.error.find("odd") != std::string::npos, "odd digits");
    r = parse_bytes("01 zz");
    check(!r.ok && r.error_pos == 3, "bad digit pos");
    r = parse_bytes("\"AB\\r\\n\\x00\"");
    check(r.ok && r.text && r.bytes == std::vector<uint8_t>({'A', 'B', '\r', '\n', 0}), "text with escapes");
    r = parse_bytes("\"open");
    check(r.ok && r.bytes.size() == 4, "unterminated text while typing");
    r = parse_bytes("\"a\" b");
    check(!r.ok, "text after quote");
    r = parse_bytes("");
    check(r.ok && r.bytes.empty(), "empty");
}

void test_byte_orders() {
    check(reorder(0x12345678, 32, ByteOrder::AsIs) == 0x12345678, "as is");
    check(reorder(0x12345678, 32, ByteOrder::Reversed) == 0x78563412, "reversed");
    check(reorder(0x12345678, 32, ByteOrder::ByteSwap16) == 0x34127856, "byte swap 16");
    check(reorder(0x12345678, 32, ByteOrder::WordSwap16) == 0x56781234, "word swap 16");
    check(reorder(0x1234, 16, ByteOrder::WordSwap16) == 0x1234, "word swap of one word");
    check(reorder(0x1234, 16, ByteOrder::ByteSwap16) == 0x3412, "byte swap 16-bit");
    check(reorder(0x0102030405060708ULL, 64, ByteOrder::WordSwap16) == 0x0708050603040102ULL, "word swap 64");
    check(reorder(0x0102030405060708ULL, 64, ByteOrder::ByteSwap16) == 0x0201040306050807ULL, "byte swap 64");
    check(order_label(32, ByteOrder::AsIs) == "ABCD", "label ABCD");
    check(order_label(32, ByteOrder::Reversed) == "DCBA", "label DCBA");
    check(order_label(32, ByteOrder::ByteSwap16) == "BADC", "label BADC");
    check(order_label(32, ByteOrder::WordSwap16) == "CDAB", "label CDAB");
    check(order_label(64, ByteOrder::WordSwap16) == "GHEFCDAB", "label 64 " + order_label(64, ByteOrder::WordSwap16));
    check(byte_string(0x12AB, 16) == "12 AB", "byte string");
    eq("wswap(0x12345678)", 0x56781234);
    eq("bswap16(0x12345678)", 0x34127856);
    // a Modbus float sent word-swapped: 1.5 = 0x3FC00000 arrives as 0x00003FC0
    eq("wswap(0x00003FC0)", 0x3FC00000);
}

void test_session() {
    Session s;
    s.env.word = {16, true};
    s.env.input = Base::Hex;
    s.decode = Decode::Time;
    s.show_orders = true;
    check(s.submit("val = ff").ok, "session submit");
    check(!s.submit("1 +").ok, "failed submit");
    check(s.history.size() == 1, "only successes in history");
    s.crc_input = "\"hi\tthere\"";
    s.crc_selected = 3;
    const std::string text = s.serialize();
    Session t;
    t.deserialize(text);
    check(t.env.word.width == 16 && t.env.word.is_signed, "word restored");
    check(t.env.input == Base::Hex, "base restored");
    check(t.decode == Decode::Time, "decode restored");
    check(t.show_orders, "orders view restored");
    check(t.env.vars["val"] == 255 && t.ans() == 255, "vars restored");
    check(t.history.size() == 1 && t.history[0].input == "val = ff" && t.history[0].value == 255, "history restored");
    check(t.crc_input == "\"hi\tthere\"" && t.crc_selected == 3, "crc restored");
    Session u;
    u.deserialize("garbage\nword\t8\t1\n");
    check(u.env.word.width == 32, "bad magic ignored");
}

}  // namespace

int main() {
    test_literals();
    test_wrapping();
    test_operators();
    test_signed();
    test_functions();
    test_variables();
    test_format();
    test_decode();
    test_checksums();
    test_parse_bytes();
    test_byte_orders();
    test_session();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
