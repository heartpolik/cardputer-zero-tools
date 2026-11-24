#include "line_editor.h"

#include <cctype>

namespace calc {

std::string prepare_input(const std::string& input) {
    std::string s = input;

    size_t first = 0;
    while (first < s.size() && std::isspace(static_cast<unsigned char>(s[first]))) ++first;
    if (first < s.size()) {
        const char c = s[first];
        const bool power = c == '*' && first + 1 < s.size() && s[first + 1] == '*';
        if (c == '+' || c == '*' || c == '/' || c == '^' || c == '%' || power) {
            s.insert(first, "ans");
        }
    }

    int depth = 0;
    for (char c : s) {
        if (c == '(') ++depth;
        else if (c == ')' && depth > 0) --depth;
    }
    // keep a trailing '=' last: "sin(1=" -> "sin(1)="
    size_t end = s.size();
    while (end > 0 && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    const bool trailing_eq = end > 0 && s[end - 1] == '=';
    if (trailing_eq) s.erase(end - 1);
    s.append(static_cast<size_t>(depth), ')');
    if (trailing_eq) s += '=';
    return s;
}

}  // namespace calc
