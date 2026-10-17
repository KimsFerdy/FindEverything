#pragma once

#include "imgui.h"
#include "platform/Appearance.h"

namespace fe {

// colours imgui's style doesn't have a slot for
struct ThemeColors {
    ImU32 folderIcon;
    ImU32 fileIcon;
    ImU32 selectedText;
    ImVec4 dimText;
};

// scale = monitor content scale
void applyTheme(const SystemAppearance& appearance, float scale);
void loadFonts(const SystemAppearance& appearance, float scale);

const ThemeColors& themeColors();

}  // namespace fe
