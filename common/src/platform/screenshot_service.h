/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <filesystem>
#include <string>

namespace platform::screenshot {

struct CaptureResult {
    bool success{false};
    std::string path;
    std::string error;
};

// Saves to ~/Pictures/Screenshots/screenshot_<timestamp>.png
CaptureResult capture_active_screen();
CaptureResult capture_active_screen(const std::filesystem::path& path);

} // namespace platform::screenshot
