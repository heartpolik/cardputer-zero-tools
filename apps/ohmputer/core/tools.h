// Formula tools: fill in any of the inputs, get the rest (pure C++).
#pragma once

#include "eseries.h"

#include <optional>
#include <string>
#include <vector>

namespace ohm {

struct FieldDef {
    const char* label;
    const char* unit;
    bool output{false};  // computed only, never typed
    const char* hint{""};
};

struct Solution {
    std::vector<std::optional<double>> values;  // one per field (typed ones echo the input)
    std::vector<std::string> notes;
    std::string error;
};

using Inputs = std::vector<std::optional<double>>;

struct Tool {
    const char* name;   // tab / header
    const char* title;  // "Ohm's law"
    std::vector<FieldDef> fields;
    int needed;  // typed inputs that determine the rest (older ones are cleared)
    Solution (*solve)(const Inputs& in, Series series);
};

const std::vector<Tool>& tools();

// A tool's typed texts plus the order they were edited in.
class Form {
public:
    explicit Form(const Tool& tool);

    const Tool& tool() const { return *tool_; }
    const std::string& text(size_t i) const { return texts_[i]; }
    // typed (non-empty) input?
    bool is_set(size_t i) const { return !texts_[i].empty(); }
    // sets a field; when more than `needed` inputs are set the least recently
    // edited one is cleared (it becomes computed)
    void set_text(size_t i, const std::string& text);
    Solution solve(Series series) const;
    // invalid number in a typed field -> its index
    std::optional<size_t> bad_field() const;

    // persistence
    const std::vector<int>& order() const { return order_; }
    void restore(std::vector<std::string> texts, std::vector<int> order);
    const std::vector<std::string>& texts() const { return texts_; }

private:
    const Tool* tool_;
    std::vector<std::string> texts_;
    std::vector<int> order_;  // edited field indices, oldest first
};

// 1/8 W ... 5 W: the smallest common rating with 2x margin
std::string resistor_rating(double watts);
// 1.5 h -> "1 h 30 min"
std::string format_hours(double hours);

}  // namespace ohm
