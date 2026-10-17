#pragma once

#include <string>

namespace fe {

// What the desktop says the UI should look like.
struct SystemAppearance {
    bool dark = true;
    bool hasAccent = false;
    float accent[3] = {0.26f, 0.59f, 0.98f};  // RGB 0..1
    std::string fontFile;                     // TrueType file; empty = ImGui's built-in font
    float fontSizePx = 13.0f;                 // at 100% display scale
};

// Asks the OS once. Cheap enough to call at startup.
SystemAppearance querySystemAppearance();

}  // namespace fe
