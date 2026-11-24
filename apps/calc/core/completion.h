// Name completion for the input line (pure, host-tested).
//
// The identifier left of the cursor is the prefix. Space accepts the first
// candidate: functions become "name()" with the cursor inside, values just
// "name". Pressing Space again right away cycles to the next candidate.
#pragma once

#include "expr.h"
#include "line_editor.h"

#include <string>
#include <vector>

namespace calc {

struct Candidate {
    std::string name;
    bool call{false};  // function: insert "name()"
};

// Start of the identifier ending at `cursor` (== cursor when there is none).
size_t identifier_start(const std::string& text, size_t cursor);

// Candidates for `prefix`: user functions/variables first, then common
// built-ins, then the rest alphabetically. Exact matches come first.
std::vector<Candidate> complete(const std::string& prefix, const Env& env, size_t max = 12);

class Completer {
public:
    // Candidates for the current editor state (for display).
    std::vector<Candidate> suggestions(const LineEditor& ed, const Env& env) const;

    // Space pressed: completes / cycles. Returns false when there was nothing
    // to complete (the caller may then insert a plain space).
    bool accept(LineEditor& ed, const Env& env);

    // Any other edit ends a cycle.
    void reset() { active_ = false; }
    bool cycling() const { return active_; }
    size_t index() const { return index_; }

private:
    bool active_{false};
    size_t start_{0};
    size_t index_{0};
    std::string prefix_;
    std::string inserted_;
    std::string text_after_;  // editor text right after our insertion (detects foreign edits)
    std::vector<Candidate> list_;
};

}  // namespace calc
