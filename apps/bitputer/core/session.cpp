#include "session.h"

#include <cstdlib>
#include <sstream>

namespace bits {
namespace {

std::vector<std::string> split_tabs(const std::string& line, size_t max_fields) {
    std::vector<std::string> f;
    size_t start = 0;
    while (f.size() + 1 < max_fields) {
        const size_t tab = line.find('\t', start);
        if (tab == std::string::npos) break;
        f.push_back(line.substr(start, tab - start));
        start = tab + 1;
    }
    f.push_back(line.substr(start));
    return f;
}

uint64_t parse_u64(const std::string& s) { return std::strtoull(s.c_str(), nullptr, 16); }

std::string hex(uint64_t v) {
    std::ostringstream o;
    o << std::hex << v;
    return o.str();
}

}  // namespace

EvalResult Session::submit(const std::string& input) {
    EvalResult r = bits::submit(input, env);
    if (r.ok) {
        history.push_back({input, r.value});
        if (history.size() > kMaxHistory) history.erase(history.begin());
    }
    return r;
}

uint64_t Session::ans() const {
    const auto it = env.vars.find("ans");
    return it == env.vars.end() ? 0 : env.word.wrap(it->second);
}

std::string Session::serialize() const {
    std::ostringstream o;
    o << "cz-bits\t1\n";
    o << "word\t" << env.word.width << '\t' << (env.word.is_signed ? 1 : 0) << '\n';
    o << "input\t" << base_name(env.input) << '\n';
    o << "decode\t" << decode_name(decode) << '\n';
    for (const auto& [name, value] : env.vars) o << "var\t" << name << '\t' << hex(value) << '\n';
    for (const auto& h : history) o << "hist\t" << hex(h.value) << '\t' << h.input << '\n';
    o << "crcsel\t" << crc_selected << '\n';
    o << "crc\t" << crc_input << '\n';
    return o.str();
}

void Session::deserialize(const std::string& text) {
    std::istringstream in(text);
    std::string line;
    if (!std::getline(in, line) || line.rfind("cz-bits\t", 0) != 0) return;
    while (std::getline(in, line)) {
        const auto f = split_tabs(line, 3);
        const std::string& k = f[0];
        if (k == "word" && f.size() == 3) {
            const int w = std::atoi(f[1].c_str());
            if (w == 8 || w == 16 || w == 32 || w == 64) env.word.width = w;
            env.word.is_signed = f[2] == "1";
        } else if (k == "input" && f.size() >= 2) {
            for (Base b : {Base::Dec, Base::Hex, Base::Bin, Base::Oct}) {
                if (f[1] == base_name(b)) env.input = b;
            }
        } else if (k == "decode" && f.size() >= 2) {
            for (Decode d : {Decode::Chars, Decode::Float, Decode::Time}) {
                if (f[1] == decode_name(d)) decode = d;
            }
        } else if (k == "var" && f.size() == 3) {
            env.vars[f[1]] = parse_u64(f[2]);
        } else if (k == "hist" && f.size() == 3) {
            history.push_back({f[2], parse_u64(f[1])});
        } else if (k == "crcsel" && f.size() >= 2) {
            crc_selected = std::atoi(f[1].c_str());
        } else if (k == "crc") {
            crc_input = line.size() > 4 ? line.substr(4) : "";
        }
    }
    if (history.size() > kMaxHistory) history.erase(history.begin(), history.end() - kMaxHistory);
}

}  // namespace bits
