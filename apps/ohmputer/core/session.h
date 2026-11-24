// OhmPuter state that survives restarts (pure C++).
#pragma once

#include "eseries.h"
#include "marking.h"
#include "tools.h"

#include <string>
#include <vector>

namespace ohm {

struct Session {
    Session();

    Series series{Series::E24};
    std::string mark_input;
    std::vector<Color> bands;
    std::string band_value;  // value -> colours mode input
    bool band_from_value{false};
    int tool{0};
    std::vector<Form> forms;  // one per tools() entry

    std::string serialize() const;
    void deserialize(const std::string& text);
};

}  // namespace ohm
