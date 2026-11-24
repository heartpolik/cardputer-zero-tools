#include "engine.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace calc {
namespace {

std::string join(const std::vector<std::string>& items, const char* sep) {
    std::string out;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i) out += sep;
        out += items[i];
    }
    return out;
}

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    size_t start = 0;
    while (true) {
        const size_t p = s.find(sep, start);
        out.push_back(s.substr(start, p == std::string::npos ? std::string::npos : p - start));
        if (p == std::string::npos) break;
        start = p + 1;
    }
    return out;
}

std::string exact(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    return buf;
}

Result error_result(const CalcError& e) {
    Result r;
    r.ok = false;
    r.text = e.what();
    r.error_pos = e.pos();
    return r;
}

}  // namespace

std::string sanitize_line(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (char c : input) {
        if (c == '\t' || c == '\n' || c == '\r') c = ' ';
        out += c;
    }
    return out;
}

double Engine::ans() const {
    auto it = env.vars.find("ans");
    return it == env.vars.end() ? 0.0 : it->second;
}

Result Engine::submit(const std::string& raw) {
    const std::string line = sanitize_line(raw);
    Result r;
    try {
        Statement st = parse_statement(line);
        switch (st.kind) {
            case Statement::Kind::Expression: {
                const double v = env.eval(st.body);
                r.ok = true;
                r.value = v;
                r.text = format_value(v);
                env.vars["ans"] = v;
                break;
            }
            case Statement::Kind::AssignVariable: {
                if (env.funcs.count(st.name)) {
                    throw CalcError("'" + st.name + "' is a function");
                }
                const double v = env.eval(st.body);
                env.vars[st.name] = v;
                env.vars["ans"] = v;
                r.ok = true;
                r.value = v;
                r.text = st.name + " = " + format_value(v);
                break;
            }
            case Statement::Kind::DefineFunction: {
                // validate: evaluate once with zeros to catch unknown names early
                Env::Locals probe;
                for (const auto& p : st.params) probe[p] = 0.0;
                env.vars.erase(st.name);
                UserFunction previous;
                const bool had_previous = env.funcs.count(st.name) > 0;
                if (had_previous) previous = env.funcs[st.name];
                env.funcs[st.name] = UserFunction{st.params, st.body, st.body_source};
                try {
                    (void)env.eval(st.body, &probe);
                } catch (const CalcError& e) {
                    const std::string msg = e.what();
                    // math domain issues at 0 are fine; unknown names/arity are not
                    if (msg.rfind("unknown", 0) == 0 || msg.find("expects") != std::string::npos ||
                        msg.find("usage") != std::string::npos || msg.find("recursion") != std::string::npos ||
                        msg.find("needs arguments") != std::string::npos) {
                        if (had_previous) env.funcs[st.name] = previous;
                        else env.funcs.erase(st.name);
                        throw;
                    }
                }
                r.ok = true;
                r.text = st.name + "(" + join(st.params, ",") + ") defined";
                break;
            }
        }
    } catch (const CalcError& e) {
        r = error_result(e);
    }

    // failed input stays in the editor for fixing; only successes go to history
    if (r.ok && !line.empty()) {
        history.push_back(HistoryEntry{line, r.text, false});
        if (history.size() > kMaxHistory) {
            history.erase(history.begin(), history.begin() + (history.size() - kMaxHistory));
        }
    }
    return r;
}

Result Engine::preview(const std::string& raw) const {
    const std::string line = sanitize_line(raw);
    Result r;
    try {
        Statement st = parse_statement(line);
        if (st.kind == Statement::Kind::DefineFunction) {
            r.ok = true;
            r.text = "";
            return r;
        }
        const double v = env.eval(st.body);
        r.ok = true;
        r.value = v;
        r.text = format_value(v);
    } catch (const CalcError& e) {
        r = error_result(e);
    }
    return r;
}

std::optional<Plot> Engine::compile_plot(const std::string& raw, std::string* error) const {
    const std::string source = sanitize_line(raw);
    try {
        Statement st = parse_statement(source);
        NodePtr body;
        if (st.kind == Statement::Kind::AssignVariable) {
            body = st.body;  // "y = expr"
        } else if (st.kind == Statement::Kind::DefineFunction) {
            if (st.params.size() != 1) throw CalcError("plot: one variable expected");
            if (st.params[0] != "x") throw CalcError("plot: use x as the variable");
            body = st.body;
        } else if (st.body->kind == Node::Kind::Name && env.funcs.count(st.body->name)) {
            // bare user function name: f  ->  f(x)
            const auto& f = env.funcs.at(st.body->name);
            if (f.params.size() != 1) throw CalcError("plot: '" + st.body->name + "' must take one argument");
            auto call = std::make_shared<Node>();
            call->kind = Node::Kind::Call;
            call->name = st.body->name;
            auto x = std::make_shared<Node>();
            x->kind = Node::Kind::Name;
            x->name = "x";
            call->args.push_back(x);
            body = call;
        } else {
            body = st.body;
        }
        // probe evaluation to catch unknown names (domain errors are fine)
        Env::Locals locals{{"x", 0.5}};
        try {
            (void)env.eval(body, &locals);
        } catch (const CalcError& e) {
            const std::string msg = e.what();
            if (msg.rfind("unknown", 0) == 0 || msg.find("expects") != std::string::npos ||
                msg.find("needs arguments") != std::string::npos) {
                throw;
            }
        }
        return Plot{body};
    } catch (const CalcError& e) {
        if (error) *error = e.what();
        return std::nullopt;
    }
}

double Engine::eval_plot(const Plot& plot, double x) const {
    Env::Locals locals{{"x", x}};
    try {
        return env.eval(plot.body, &locals);
    } catch (const CalcError&) {
        return std::nan("");
    }
}

std::string Engine::serialize() const {
    std::ostringstream out;
    out << "cz-calc\t1\n";
    out << "angle\t" << (env.angle == AngleMode::Deg ? "deg" : "rad") << "\n";
    out << "format\t" << format_name(format) << "\n";
    for (const auto& [name, value] : env.vars) {
        out << "var\t" << name << "\t" << exact(value) << "\n";
    }
    for (const auto& [name, f] : env.funcs) {
        out << "fn\t" << name << "\t" << join(f.params, ",") << "\t" << f.source << "\n";
    }
    for (size_t i = 0; i < plots.size(); ++i) {
        out << "plot\t" << i << "\t" << (plot_enabled[i] ? 1 : 0) << "\t" << plots[i] << "\n";
    }
    for (const auto& h : history) {
        out << "hist\t" << (h.error ? 1 : 0) << "\t" << h.input << "\t" << h.output << "\n";
    }
    return out.str();
}

void Engine::deserialize(const std::string& data) {
    std::istringstream in(data);
    std::string line;
    std::vector<std::vector<std::string>> fn_lines;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto f = split(line, '\t');
        if (f.empty()) continue;
        const auto& kind = f[0];
        if (kind == "angle" && f.size() >= 2) {
            env.angle = f[1] == "deg" ? AngleMode::Deg : AngleMode::Rad;
        } else if (kind == "format" && f.size() >= 2) {
            format = f[1] == "SCI" ? NumberFormat::Sci : f[1] == "ENG" ? NumberFormat::Eng : NumberFormat::Auto;
        } else if (kind == "var" && f.size() >= 3) {
            env.vars[f[1]] = std::strtod(f[2].c_str(), nullptr);
        } else if (kind == "fn" && f.size() >= 4) {
            fn_lines.push_back(f);
        } else if (kind == "plot" && f.size() >= 4) {
            const size_t i = static_cast<size_t>(std::atoi(f[1].c_str()));
            if (i < plots.size()) {
                plot_enabled[i] = f[2] == "1";
                plots[i] = f[3];
            }
        } else if (kind == "hist" && f.size() >= 4) {
            history.push_back(HistoryEntry{f[2], f[3], f[1] == "1"});
        }
    }
    // functions may reference each other: parse all, no probing
    for (const auto& f : fn_lines) {
        try {
            UserFunction uf;
            uf.params = split(f[2], ',');
            uf.source = f[3];
            uf.body = parse_expression(f[3]);
            env.funcs[f[1]] = uf;
        } catch (const CalcError&) {
            // skip corrupted entry
        }
    }
    if (history.size() > kMaxHistory) {
        history.erase(history.begin(), history.begin() + (history.size() - kMaxHistory));
    }
}

}  // namespace calc
