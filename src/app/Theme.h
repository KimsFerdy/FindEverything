#pragma once

#include "platform/Appearance.h"

#include "imgui.h"

namespace fe {

// Colours the app derives from the system appearance, for things ImGui's style
// doesn't cover (icons, selected-row text...).
struct ThemeColors {
    ImU32 folderIcon;
    ImU32 fileIcon;
    ImU32 selectedText;
    ImVec4 dimText;
};

// scale = monitor content scale (1.0 normal, 2.0 HiDPI). Call before the first frame.
void applyTheme(const SystemAppearance& appearance, float scale);
void loadFonts(const SystemAppearance& appearance, float scale);

const ThemeColors& themeColors();

}  // namespace fe
