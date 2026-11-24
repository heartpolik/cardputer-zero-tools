// Single-line ASCII editor with a cursor (pure, host-tested).
#pragma once

#include <cstddef>
#include <string>

namespace calc {

class LineEditor {
public:
    const std::string& text() const { return text_; }
    size_t cursor() const { return cursor_; }
    bool empty() const { return text_.empty(); }

    void set(std::string text) {
        text_ = std::move(text);
        cursor_ = text_.size();
    }
    void clear() { set(""); }
    void insert(const std::string& s) {
        text_.insert(cursor_, s);
        cursor_ += s.size();
    }
    void insert(char c) { insert(std::string(1, c)); }
    void backspace() {
        if (cursor_ == 0) return;
        text_.erase(cursor_ - 1, 1);
        --cursor_;
    }
    void del() {
        if (cursor_ < text_.size()) text_.erase(cursor_, 1);
    }
    void left() { if (cursor_ > 0) --cursor_; }
    void right() { if (cursor_ < text_.size()) ++cursor_; }
    void home() { cursor_ = 0; }
    void end() { cursor_ = text_.size(); }
    void move_to(size_t pos) { cursor_ = pos > text_.size() ? text_.size() : pos; }

    // Replaces [pos, pos+len) with s and puts the cursor at pos + cursor_offset.
    void replace(size_t pos, size_t len, const std::string& s, size_t cursor_offset) {
        if (pos > text_.size()) pos = text_.size();
        text_.replace(pos, len, s);
        move_to(pos + cursor_offset);
    }

    // Typing with paired parentheses:
    //  '(' inserts "()" with the cursor inside (unless an identifier/number follows),
    //  ')' steps over an existing ')' instead of inserting another one.
    void type(char c);
    // Backspace that also removes the ')' of an empty "()" pair.
    void smart_backspace();

private:
    std::string text_;
    size_t cursor_{0};
};

// Calculator conveniences applied before evaluation:
//  * a leading binary operator continues from the previous answer: "*2" -> "ans*2"
//  * missing closing parentheses are appended: "sin(pi/2" -> "sin(pi/2)"
std::string prepare_input(const std::string& input);

}  // namespace calc
