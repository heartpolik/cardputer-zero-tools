// Expression language of the calculator: lexer, parser (to AST) and evaluator.
// Pure C++17, no LVGL — compiled and tested on the host.
#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace calc {

enum class AngleMode { Rad, Deg };

// Any parse or evaluation error. `pos` is a byte offset into the source
// (std::string::npos when the error has no meaningful position).
class CalcError : public std::runtime_error {
public:
    CalcError(const std::string& message, size_t pos = std::string::npos)
        : std::runtime_error(message), pos_(pos) {}
    size_t pos() const { return pos_; }

private:
    size_t pos_;
};

struct Node;
using NodePtr = std::shared_ptr<const Node>;

struct Node {
    enum class Kind { Number, Name, Unary, Binary, Postfix, Call };
    Kind kind{Kind::Number};
    double number{0.0};
    std::string name;   // Name / Call
    char op{0};         // Unary: '-' '+'; Binary: + - * / % ^; Postfix: '!'
    std::vector<NodePtr> args;
    size_t pos{0};
};

struct Statement {
    enum class Kind { Expression, AssignVariable, DefineFunction };
    Kind kind{Kind::Expression};
    std::string name;                 // variable / function name
    std::vector<std::string> params;  // DefineFunction
    NodePtr body;
    std::string body_source;          // text of the right-hand side (for persistence)
};

// Parses one input line: `expr`, `name = expr` or `f(x, y) = expr`.
// A single trailing '=' is tolerated ("2+2=").
Statement parse_statement(std::string_view source);

// Parses an expression only (used for plotting).
NodePtr parse_expression(std::string_view source);

// Numeric derivative of f at a (used by der() and the graph trace).
double numeric_derivative(const std::function<double(double)>& f, double a);

struct UserFunction {
    std::vector<std::string> params;
    NodePtr body;
    std::string source;  // body source text
};

// Evaluation environment: variables, user functions and angle mode.
class Env {
public:
    AngleMode angle{AngleMode::Rad};
    std::map<std::string, double> vars;
    std::map<std::string, UserFunction> funcs;

    using Locals = std::map<std::string, double>;

    double eval(const NodePtr& node) const { return eval(node, nullptr, 0); }
    double eval(const NodePtr& node, const Locals* locals) const { return eval(node, locals, 0); }

    // Names that cannot be assigned or redefined (built-in functions/constants).
    static bool is_reserved(const std::string& name);
    static bool is_builtin_function(const std::string& name);
    static std::vector<std::string> builtin_names();

private:
    double eval(const NodePtr& node, const Locals* locals, int depth) const;
    double call(const Node& node, const Locals* locals, int depth) const;
    double call_special(const Node& node, const Locals* locals, int depth) const;
    double lookup(const Node& node, const Locals* locals) const;
};

}  // namespace calc
