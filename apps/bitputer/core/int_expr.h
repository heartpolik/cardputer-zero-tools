// Integer expression language of BitPuter (pure C++, host-tested).
//
// Every operation wraps to the current word (8/16/32/64 bits, signed or not),
// like a register would. C precedence, low to high:
//   |   ^   &   << >> >>>   + -   * / %   unary - ~   **
// Literals: input-base digits (DEC: 255, HEX: ff), 0x1F, 0o17, 0b101 (not in HEX
// mode, where 0b1 is hex), 0n255 (decimal in any mode), 'A' / 'RIFF' (chars),
// 1_000 (separators), 4K 2M 1G (binary multiples), f32(1.5) f64(-0.1) (IEEE bits).
// In HEX mode a word made only of hex digits (ff, cafe) is a number, so
// variable names need another letter: x = 5, val = 0x10.
#pragma once

#include "word.h"

#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace bits {

class ExprError : public std::runtime_error {
public:
    ExprError(const std::string& msg, size_t pos) : std::runtime_error(msg), pos_(pos) {}
    size_t pos() const { return pos_; }

private:
    size_t pos_;
};

struct Env {
    Word word;
    Base input{Base::Dec};
    std::map<std::string, uint64_t> vars;  // includes "ans"
};

struct EvalResult {
    bool ok{false};
    uint64_t value{0};
    std::string assigned;  // variable name for "name = expr"
    std::string error;
    size_t error_pos{std::string::npos};
};

// Evaluates without changing env (preview). Assignments are reported, not applied.
EvalResult evaluate(const std::string& src, const Env& env);
// Evaluates and applies: assigns the variable (if any) and ans.
EvalResult submit(const std::string& src, Env& env);

struct FunctionInfo {
    const char* name;
    const char* args;
    const char* help;
};
const std::vector<FunctionInfo>& functions();

}  // namespace bits
