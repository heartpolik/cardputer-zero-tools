// BitPuter state that survives restarts (pure C++).
#pragma once

#include "decode.h"
#include "int_expr.h"

#include <string>
#include <vector>

namespace bits {

struct HistoryEntry {
    std::string input;
    uint64_t value{0};
};

struct Session {
    static constexpr size_t kMaxHistory = 100;

    Env env;
    Decode decode{Decode::Chars};
    bool show_orders{false};  // byte-order table instead of the bit grid
    std::vector<HistoryEntry> history;
    std::string crc_input;
    int crc_selected{0};

    // evaluates, records history and ans
    EvalResult submit(const std::string& input);
    uint64_t ans() const;

    std::string serialize() const;
    void deserialize(const std::string& text);
};

}  // namespace bits
