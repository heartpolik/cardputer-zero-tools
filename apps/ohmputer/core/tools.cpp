#include "tools.h"

#include "si.h"

#include <algorithm>
#include <cmath>

namespace ohm {
namespace {

constexpr double kPi = 3.14159265358979323846;
const char* const kOhm = "\xCE\xA9";

bool has(const Inputs& in, size_t i) { return in[i].has_value(); }
double at(const Inputs& in, size_t i) { return *in[i]; }

Solution start(const Inputs& in) {
    Solution s;
    s.values = in;
    return s;
}

std::string pct(double err) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%+.2f %%", err * 100);
    return buf;
}

// ---- Ohm's law: V I R P
Solution solve_ohm(const Inputs& in, Series series) {
    auto s = start(in);
    auto& v = s.values;
    const bool V = has(in, 0), I = has(in, 1), R = has(in, 2), P = has(in, 3);
    if (V && I) v[2] = at(in, 0) / at(in, 1), v[3] = at(in, 0) * at(in, 1);
    else if (V && R) v[1] = at(in, 0) / at(in, 2), v[3] = at(in, 0) * at(in, 0) / at(in, 2);
    else if (V && P) v[1] = at(in, 3) / at(in, 0), v[2] = at(in, 0) * at(in, 0) / at(in, 3);
    else if (I && R) v[0] = at(in, 1) * at(in, 2), v[3] = at(in, 1) * at(in, 1) * at(in, 2);
    else if (I && P) v[0] = at(in, 3) / at(in, 1), v[2] = at(in, 3) / (at(in, 1) * at(in, 1));
    else if (R && P) v[0] = std::sqrt(at(in, 3) * at(in, 2)), v[1] = std::sqrt(at(in, 3) / at(in, 2));
    else {
        s.notes.push_back("enter any two of V I R P");
        return s;
    }
    if (!R && v[2] && *v[2] > 0) {
        const double n = nearest(*v[2], series);
        s.notes.push_back(std::string(series_name(series)) + ": " + format_si(n, kOhm) + "  (" + pct(n / *v[2] - 1) + ")");
    }
    if (v[3] && *v[3] > 0) s.notes.push_back("resistor rating: " + resistor_rating(*v[3]));
    return s;
}

// ---- voltage divider: Vin R1 R2 Vout | I
Solution solve_divider(const Inputs& in, Series series) {
    auto s = start(in);
    auto& v = s.values;
    const bool Vin = has(in, 0), R1 = has(in, 1), R2 = has(in, 2), Vout = has(in, 3);
    if (Vin && Vout && at(in, 3) >= at(in, 0)) {
        s.error = "Vout must be below Vin";
        return s;
    }
    if (Vin && R1 && R2) v[3] = at(in, 0) * at(in, 2) / (at(in, 1) + at(in, 2));
    else if (Vin && R1 && Vout) v[2] = at(in, 1) * at(in, 3) / (at(in, 0) - at(in, 3));
    else if (Vin && R2 && Vout) v[1] = at(in, 2) * (at(in, 0) - at(in, 3)) / at(in, 3);
    else if (R1 && R2 && Vout) v[0] = at(in, 3) * (at(in, 1) + at(in, 2)) / at(in, 2);
    if (v[0] && v[1] && v[2]) v[4] = *v[0] / (*v[1] + *v[2]);
    if (Vin && Vout && !(R1 && R2)) {
        const auto pair = divider_pair(at(in, 3) / at(in, 0), series);
        if (pair.r1 > 0) {
            s.notes.push_back(std::string(series_name(series)) + ": R1 " + format_si(pair.r1, kOhm, 3) + "  R2 " +
                              format_si(pair.r2, kOhm, 3));
            s.notes.push_back("  -> " + format_si(at(in, 0) * pair.ratio, "V") + "  (" +
                              pct(pair.ratio / (at(in, 3) / at(in, 0)) - 1) + ")");
        }
    } else if (!v[3] || !v[0]) {
        s.notes.push_back("enter 3 of Vin R1 R2 Vout (or Vin + Vout)");
    }
    return s;
}

// ---- LED series resistor: Vs Vf If R | P(R) P(LED)
Solution solve_led(const Inputs& in, Series series) {
    auto s = start(in);
    auto& v = s.values;
    const bool Vs = has(in, 0), Vf = has(in, 1), If = has(in, 2), R = has(in, 3);
    if (Vs && Vf && at(in, 1) >= at(in, 0)) {
        s.error = "supply must be above Vf";
        return s;
    }
    if (Vs && Vf && If) v[3] = (at(in, 0) - at(in, 1)) / at(in, 2);
    else if (Vs && Vf && R) v[2] = (at(in, 0) - at(in, 1)) / at(in, 3);
    else if (Vs && If && R) v[1] = at(in, 0) - at(in, 2) * at(in, 3);
    else if (Vf && If && R) v[0] = at(in, 1) + at(in, 2) * at(in, 3);
    else {
        s.notes.push_back("Vf: red 2.0  green 2.2  blue/white 3.0");
        return s;
    }
    if (v[2] && v[3]) v[4] = *v[2] * *v[2] * *v[3];
    if (v[1] && v[2]) v[5] = *v[1] * *v[2];
    if (!R && v[3] && *v[3] > 0) {
        const double n = nearest(*v[3], series, +1);  // round up: less current
        const double i = (*v[0] - *v[1]) / n;
        s.notes.push_back(std::string(series_name(series)) + " up: " + format_si(n, kOhm) + " -> " + format_si(i, "A", 3) +
                          ", " + resistor_rating(i * i * n));
    }
    return s;
}

// ---- RC: R C tau fc
Solution solve_rc(const Inputs& in, Series series) {
    auto s = start(in);
    auto& v = s.values;
    const bool R = has(in, 0), C = has(in, 1), T = has(in, 2), F = has(in, 3);
    if (T && F) {
        s.error = "tau and fc are the same thing: clear one";
        return s;
    }
    double tau = 0;
    if (T) tau = at(in, 2);
    else if (F) tau = 1 / (2 * kPi * at(in, 3));
    else if (R && C) tau = at(in, 0) * at(in, 1);
    else {
        s.notes.push_back("enter two: R C, R tau, C fc ...");
        return s;
    }
    if (!R && C) v[0] = tau / at(in, 1);
    else if (R && !C) v[1] = tau / at(in, 0);
    v[2] = tau;
    v[3] = 1 / (2 * kPi * tau);
    s.notes.push_back("63% " + format_si(tau, "s", 3) + "  95% " + format_si(3 * tau, "s", 3) + "  99% " +
                      format_si(5 * tau, "s", 3));
    if (!R && v[0]) {
        s.notes.push_back(std::string(series_name(series)) + ": R " + format_si(nearest(*v[0], series), kOhm));
    }
    return s;
}

// ---- LC resonance: L C f0 | Z0
Solution solve_lc(const Inputs& in, Series) {
    auto s = start(in);
    auto& v = s.values;
    const bool L = has(in, 0), C = has(in, 1), F = has(in, 2);
    const double w2 = F ? std::pow(2 * kPi * at(in, 2), 2) : 0;
    if (L && C) v[2] = 1 / (2 * kPi * std::sqrt(at(in, 0) * at(in, 1)));
    else if (F && L) v[1] = 1 / (w2 * at(in, 0));
    else if (F && C) v[0] = 1 / (w2 * at(in, 1));
    else {
        s.notes.push_back("enter two of L C f0");
        return s;
    }
    v[3] = std::sqrt(*v[0] / *v[1]);
    s.notes.push_back("X = " + format_si(2 * kPi * *v[2] * *v[0], kOhm) + " at f0");
    return s;
}

// ---- battery runtime: Ah V W eff DoD | Wh I h
Solution solve_battery(const Inputs& in, Series) {
    auto s = start(in);
    auto& v = s.values;
    if (!has(in, 0) || !has(in, 1)) {
        s.notes.push_back("capacity and voltage, then the load");
        return s;
    }
    const double eff = has(in, 3) ? at(in, 3) / 100 : 1.0;
    const double dod = has(in, 4) ? at(in, 4) / 100 : 1.0;
    if (eff <= 0 || eff > 1 || dod <= 0 || dod > 1) {
        s.error = "efficiency and DoD are 1..100 %";
        return s;
    }
    const double wh = at(in, 0) * at(in, 1);
    v[5] = wh * dod * eff;
    s.notes.push_back("stored " + format_si(wh, "Wh", 4) + "  usable after DoD " + format_si(wh * dod, "Wh", 4));
    if (has(in, 2) && at(in, 2) > 0) {
        v[6] = at(in, 2) / eff / at(in, 1);
        v[7] = *v[5] / at(in, 2);
        s.notes.push_back("runtime " + format_hours(*v[7]));
    }
    return s;
}

const std::vector<Tool> kTools = {
    {"OHM", "Ohm's law", {{"V", "V"}, {"I", "A"}, {"R", kOhm}, {"P", "W"}}, 2, solve_ohm},
    {"DIV", "Voltage divider",
     {{"Vin", "V"}, {"R1 top", kOhm}, {"R2 bottom", kOhm}, {"Vout", "V"}, {"I", "A", true}}, 3, solve_divider},
    {"LED", "LED resistor",
     {{"Supply", "V"}, {"Vf", "V"}, {"If", "A"}, {"R", kOhm}, {"P resistor", "W", true}, {"P LED", "W", true}}, 3,
     solve_led},
    {"RC", "RC filter / timer", {{"R", kOhm}, {"C", "F"}, {"tau", "s"}, {"fc", "Hz"}}, 2, solve_rc},
    {"LC", "LC resonance", {{"L", "H"}, {"C", "F"}, {"f0", "Hz"}, {"Z0", kOhm, true}}, 2, solve_lc},
    {"BATT", "Battery runtime",
     {{"Capacity", "Ah"},
      {"Voltage", "V"},
      {"Load", "W"},
      {"Efficiency", "%"},
      {"DoD", "%"},
      {"Usable", "Wh", true},
      {"Current", "A", true},
      {"Runtime", "h", true}},
     5, solve_battery},
};

}  // namespace

const std::vector<Tool>& tools() { return kTools; }

Form::Form(const Tool& tool) : tool_(&tool), texts_(tool.fields.size()) {}

void Form::set_text(size_t i, const std::string& text) {
    if (i >= texts_.size() || tool_->fields[i].output) return;
    texts_[i] = text;
    order_.erase(std::remove(order_.begin(), order_.end(), static_cast<int>(i)), order_.end());
    if (text.empty()) return;
    order_.push_back(static_cast<int>(i));
    while (static_cast<int>(order_.size()) > tool_->needed) {
        texts_[static_cast<size_t>(order_.front())].clear();
        order_.erase(order_.begin());
    }
}

std::optional<size_t> Form::bad_field() const {
    for (size_t i = 0; i < texts_.size(); ++i) {
        if (!texts_[i].empty() && !parse_si(texts_[i])) return i;
    }
    return std::nullopt;
}

Solution Form::solve(Series series) const {
    Inputs in(texts_.size());
    for (size_t i = 0; i < texts_.size(); ++i) {
        if (!texts_[i].empty()) {
            in[i] = parse_si(texts_[i]);
            if (in[i] && *in[i] <= 0 && std::string(tool_->fields[i].unit) != "%") in[i].reset();
        }
    }
    if (const auto bad = bad_field()) {
        Solution s;
        s.values = in;
        s.error = std::string("can't read ") + tool_->fields[*bad].label + ": " + texts_[*bad];
        return s;
    }
    for (size_t i = 0; i < in.size(); ++i) {
        if (!texts_[i].empty() && !in[i]) {
            Solution s;
            s.values = in;
            s.error = std::string(tool_->fields[i].label) + " must be > 0";
            return s;
        }
    }
    Solution s = tool_->solve(in, series);
    for (auto& v : s.values) {
        if (v && !std::isfinite(*v)) v.reset();
    }
    return s;
}

void Form::restore(std::vector<std::string> texts, std::vector<int> order) {
    texts.resize(texts_.size());
    texts_ = std::move(texts);
    order_.clear();
    for (int i : order) {
        if (i >= 0 && static_cast<size_t>(i) < texts_.size() && !texts_[static_cast<size_t>(i)].empty()) order_.push_back(i);
    }
    for (size_t i = 0; i < texts_.size(); ++i) {
        if (tool_->fields[i].output) texts_[i].clear();
        else if (!texts_[i].empty() && std::find(order_.begin(), order_.end(), static_cast<int>(i)) == order_.end())
            order_.push_back(static_cast<int>(i));
    }
}

std::string resistor_rating(double watts) {
    static const std::pair<double, const char*> ratings[] = {
        {0.125, "1/8 W"}, {0.25, "1/4 W"}, {0.5, "1/2 W"}, {1, "1 W"}, {2, "2 W"}, {3, "3 W"}, {5, "5 W"}};
    for (const auto& [w, name] : ratings) {
        if (watts * 2 <= w) return name;
    }
    return "> 5 W (" + format_si(watts, "W", 3) + ")";
}

std::string format_hours(double hours) {
    if (!(hours >= 0) || !std::isfinite(hours)) return "--";
    if (hours >= 48) return format_plain(hours / 24, 3) + " days";
    const long minutes = std::lround(hours * 60);
    const long h = minutes / 60, m = minutes % 60;
    if (h == 0) return std::to_string(m) + " min";
    return std::to_string(h) + " h " + std::to_string(m) + " min";
}

}  // namespace ohm
