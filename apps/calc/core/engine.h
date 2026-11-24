// Calculator session: history, `ans`, variables/functions, plot slots, persistence.
#pragma once

#include "expr.h"
#include "format.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace calc {

struct HistoryEntry {
    std::string input;
    std::string output;
    bool error{false};
};

struct Result {
    bool ok{false};
    std::string text;               // formatted value or error message
    double value{0.0};
    size_t error_pos{std::string::npos};
};

// A compiled plot expression: evaluate `body` with the variable `x` bound.
struct Plot {
    NodePtr body;
};

class Engine {
public:
    static constexpr size_t kMaxHistory = 200;
    static constexpr size_t kPlotSlots = 4;

    Env env;
    NumberFormat format{NumberFormat::Auto};
    std::vector<HistoryEntry> history;
    std::array<std::string, kPlotSlots> plots{};
    std::array<bool, kPlotSlots> plot_enabled{true, true, true, true};

    // Executes a line: evaluates, assigns or defines, updates `ans`.
    // Successful lines are appended to history.
    Result submit(const std::string& line);

    // Side-effect free evaluation for live preview. Returns ok=false for
    // incomplete or invalid input (text then holds the error).
    Result preview(const std::string& line) const;

    // Compiles a plot slot: `expr in x`, `y = expr`, or a user function name `f`.
    std::optional<Plot> compile_plot(const std::string& source, std::string* error = nullptr) const;
    double eval_plot(const Plot& plot, double x) const;

    double ans() const;
    std::string format_value(double v) const { return format_number(v, format); }

    std::string serialize() const;
    void deserialize(const std::string& data);
};

// Removes characters that would break the line-based state file.
std::string sanitize_line(const std::string& input);

}  // namespace calc
