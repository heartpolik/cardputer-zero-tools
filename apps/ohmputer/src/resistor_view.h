#pragma once

#include "app_context.h"
#include "marking.h"

#include <vector>

namespace ohm_ui {

// A through-hole resistor drawn with its colour bands (children of `parent`).
// Draws into a fresh container that is returned; delete it to redraw.
lv_obj_t* draw_resistor(lv_obj_t* parent, const Style& s, int x, int y, int body_w, int body_h,
                        const std::vector<ohm::Color>& bands, bool with_leads = true);

}  // namespace ohm_ui
