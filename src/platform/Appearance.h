#pragma once

#include <string>

namespace fe {

// what the desktop wants us to look like
struct SystemAppearance {
    bool dark = true;
    bool hasAccent = false;
    float accent[3] = {0.26f, 0.59f, 0.98f};
    std::string fontFile;      // ttf, empty = imgui default font
    float fontSizePx = 13.0f;  // at 100% scale
    std::string iconTheme;     // "Mint-Y-Aqua"
};

SystemAppearance querySystemAppearance();

}  // namespace fe
