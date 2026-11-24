#include "session.h"

#include <cstdlib>
#include <sstream>

namespace ohm {
namespace {

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    size_t start = 0;
    for (;;) {
        const size_t p = s.find(sep, start);
        out.push_back(s.substr(start, p == std::string::npos ? std::string::npos : p - start));
        if (p == std::string::npos) return out;
        start = p + 1;
    }
}

}  // namespace

Session::Session() {
    for (const auto& t : tools()) forms.emplace_back(t);
    // a typical 12 V LiFePO4 pack until the user types their own
    forms[5].restore({"100", "12.8", "60", "90", "90"}, {0, 1, 2, 3, 4});
}

std::string Session::serialize() const {
    std::ostringstream o;
    o << "cz-ohm\t1\n";
    o << "series\t" << series_name(series) << '\n';
    o << "tool\t" << tool << '\n';
    o << "mark\t" << mark_input << '\n';
    o << "bandmode\t" << (band_from_value ? 1 : 0) << '\n';
    o << "bandvalue\t" << band_value << '\n';
    o << "bands\t";
    for (size_t i = 0; i < bands.size(); ++i) o << (i ? "," : "") << static_cast<int>(bands[i]);
    o << '\n';
    for (size_t t = 0; t < forms.size(); ++t) {
        o << "form\t" << t;
        for (const auto& text : forms[t].texts()) o << '\t' << text;
        o << '\n' << "order\t" << t;
        for (int i : forms[t].order()) o << '\t' << i;
        o << '\n';
    }
    return o.str();
}

void Session::deserialize(const std::string& text) {
    std::istringstream in(text);
    std::string line;
    if (!std::getline(in, line) || line.rfind("cz-ohm\t", 0) != 0) return;
    std::vector<std::vector<std::string>> texts(forms.size());
    std::vector<std::vector<int>> orders(forms.size());
    std::vector<bool> seen(forms.size(), false);
    while (std::getline(in, line)) {
        const auto f = split(line, '\t');
        const std::string& k = f[0];
        const std::string rest = line.size() > k.size() + 1 ? line.substr(k.size() + 1) : "";
        if (k == "series") {
            for (Series s : {Series::E6, Series::E12, Series::E24, Series::E48, Series::E96}) {
                if (rest == series_name(s)) series = s;
            }
        } else if (k == "tool") {
            tool = std::atoi(rest.c_str());
        } else if (k == "mark") {
            mark_input = rest;
        } else if (k == "bandmode") {
            band_from_value = rest == "1";
        } else if (k == "bandvalue") {
            band_value = rest;
        } else if (k == "bands") {
            bands.clear();
            if (!rest.empty()) {
                for (const auto& c : split(rest, ',')) {
                    const int v = std::atoi(c.c_str());
                    if (v >= 0 && v <= static_cast<int>(Color::Silver) && bands.size() < 6) bands.push_back(static_cast<Color>(v));
                }
            }
        } else if ((k == "form" || k == "order") && f.size() >= 2) {
            const int t = std::atoi(f[1].c_str());
            if (t < 0 || static_cast<size_t>(t) >= forms.size()) continue;
            if (k == "form") {
                texts[static_cast<size_t>(t)].assign(f.begin() + 2, f.end());
                seen[static_cast<size_t>(t)] = true;
            } else {
                for (size_t i = 2; i < f.size(); ++i) orders[static_cast<size_t>(t)].push_back(std::atoi(f[i].c_str()));
            }
        }
    }
    for (size_t t = 0; t < forms.size(); ++t) {
        if (seen[t]) forms[t].restore(texts[t], orders[t]);
    }
    if (tool < 0 || static_cast<size_t>(tool) >= forms.size()) tool = 0;
}

}  // namespace ohm
