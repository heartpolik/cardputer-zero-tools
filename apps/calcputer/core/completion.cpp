#include "completion.h"

#include <algorithm>
#include <cctype>
#include <set>

namespace calc {
namespace {

// Most used first; everything else follows alphabetically.
const std::vector<std::string>& common_order() {
    static const std::vector<std::string> order = {
        "sin", "cos", "tan", "sqrt", "ln", "log", "exp", "abs", "ans", "pi", "e",
        "asin", "acos", "atan", "der", "integ", "solve", "sum", "round", "floor", "ceil",
        "sinh", "cosh", "tanh", "root", "cbrt", "log2", "log10", "min", "max", "mod",
    };
    return order;
}

bool ident_char(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

}  // namespace

size_t identifier_start(const std::string& text, size_t cursor) {
    size_t i = std::min(cursor, text.size());
    while (i > 0 && ident_char(text[i - 1])) --i;
    // identifiers start with a letter: "2si" -> "si", "x2" stays "x2", "12" -> none
    while (i < cursor && std::isdigit(static_cast<unsigned char>(text[i]))) ++i;
    return i;
}

std::vector<Candidate> complete(const std::string& prefix, const Env& env, size_t max) {
    std::vector<Candidate> out;
    if (prefix.empty()) return out;
    std::set<std::string> seen;
    auto starts = [&](const std::string& name) { return name.compare(0, prefix.size(), prefix) == 0; };
    auto add = [&](const std::string& name, bool call) {
        if (!starts(name) || !seen.insert(name).second) return;
        out.push_back({name, call});
    };

    for (const auto& [name, _] : env.funcs) add(name, true);
    for (const auto& [name, _] : env.vars) add(name, false);
    const auto is_value = [](const std::string& n) {
        return n == "pi" || n == "e" || n == "tau" || n == "phi" || n == "inf" || n == "ans";
    };
    for (const auto& name : common_order()) add(name, !is_value(name));
    for (const auto& name : Env::builtin_names()) add(name, true);
    for (const char* c : {"tau", "phi", "inf", "ans"}) add(c, false);

    // an exact match wins (typing "sin" should not offer "sinh" first)
    std::stable_partition(out.begin(), out.end(), [&](const Candidate& c) { return c.name == prefix; });
    if (out.size() > max) out.resize(max);
    return out;
}

std::vector<Candidate> Completer::suggestions(const LineEditor& ed, const Env& env) const {
    if (active_) return list_;
    const size_t start = identifier_start(ed.text(), ed.cursor());
    if (start == ed.cursor()) return {};
    // only complete at the end of an identifier, not in the middle of one
    if (ed.cursor() < ed.text().size() && ident_char(ed.text()[ed.cursor()])) return {};
    return complete(ed.text().substr(start, ed.cursor() - start), env);
}

bool Completer::accept(LineEditor& ed, const Env& env) {
    if (active_ && ed.text() == text_after_ && !list_.empty()) {
        // cycle: replace our previous insertion with the next candidate
        index_ = (index_ + 1) % list_.size();
        const auto& c = list_[index_];
        const std::string text = c.call ? c.name + "()" : c.name;
        ed.replace(start_, inserted_.size(), text, c.call ? c.name.size() + 1 : c.name.size());
        inserted_ = text;
        text_after_ = ed.text();
        return true;
    }

    active_ = false;
    const auto list = suggestions(ed, env);
    if (list.empty()) return false;
    start_ = identifier_start(ed.text(), ed.cursor());
    prefix_ = ed.text().substr(start_, ed.cursor() - start_);
    list_ = list;
    index_ = 0;

    const auto& c = list_[0];
    // a function already followed by '(' only needs the name
    const bool has_paren = ed.cursor() < ed.text().size() && ed.text()[ed.cursor()] == '(';
    const std::string text = c.call && !has_paren ? c.name + "()" : c.name;
    ed.replace(start_, prefix_.size(), text, c.call && !has_paren ? c.name.size() + 1 : c.name.size());
    inserted_ = text;
    text_after_ = ed.text();
    active_ = !has_paren && list_.size() > 1;
    return true;
}

}  // namespace calc
