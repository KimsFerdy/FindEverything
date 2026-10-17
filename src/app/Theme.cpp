#include "app/Theme.h"

#include <cmath>

namespace fe {

namespace {

ThemeColors g_colors;

ImVec4 hex(unsigned rgb, float a = 1.0f) {
    return ImVec4(((rgb >> 16) & 0xFF) / 255.0f, ((rgb >> 8) & 0xFF) / 255.0f, (rgb & 0xFF) / 255.0f, a);
}

ImVec4 mix(const ImVec4& a, const ImVec4& b, float t) {
    return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}

ImVec4 alpha(ImVec4 c, float a) {
    c.w = a;
    return c;
}

// Base surfaces, modelled on Mint-Y / Adwaita.
struct Palette {
    ImVec4 bg, surface, input, border, text, dim;
};

Palette darkPalette() {
    return {hex(0x2B2B2B), hex(0x363636), hex(0x212121), hex(0x1A1A1A), hex(0xDADADA), hex(0x8C8C8C)};
}

Palette lightPalette() {
    return {hex(0xF5F5F5), hex(0xEBEBEB), hex(0xFFFFFF), hex(0xCDCDCD), hex(0x2E2E2E), hex(0x7A7A7A)};
}

}  // namespace

void applyTheme(const SystemAppearance& ap, float scale) {
    ImGuiStyle style;
    if (ap.dark) ImGui::StyleColorsDark(&style);
    else ImGui::StyleColorsLight(&style);

    style.WindowPadding = ImVec2(10, 10);
    style.FramePadding = ImVec2(10, 6);
    style.ItemSpacing = ImVec2(8, 8);
    style.CellPadding = ImVec2(8, 4);
    style.WindowRounding = 0;
    style.ChildRounding = 4;
    style.FrameRounding = 6;
    style.PopupRounding = 6;
    style.ScrollbarRounding = 8;
    style.GrabRounding = 4;
    style.WindowBorderSize = 0;
    style.FrameBorderSize = 1;
    style.PopupBorderSize = 1;
    style.ScrollbarSize = 12;

    const Palette p = ap.dark ? darkPalette() : lightPalette();
    const ImVec4 accent(ap.accent[0], ap.accent[1], ap.accent[2], 1.0f);

    ImVec4* c = style.Colors;
    c[ImGuiCol_Text] = p.text;
    c[ImGuiCol_TextDisabled] = p.dim;
    c[ImGuiCol_WindowBg] = p.bg;
    c[ImGuiCol_ChildBg] = p.bg;
    c[ImGuiCol_PopupBg] = p.surface;
    c[ImGuiCol_Border] = p.border;
    c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg] = p.input;
    c[ImGuiCol_FrameBgHovered] = mix(p.input, accent, 0.06f);
    c[ImGuiCol_FrameBgActive] = mix(p.input, accent, 0.10f);
    c[ImGuiCol_TitleBg] = p.surface;
    c[ImGuiCol_TitleBgActive] = p.surface;
    c[ImGuiCol_TitleBgCollapsed] = p.surface;
    c[ImGuiCol_MenuBarBg] = p.surface;
    c[ImGuiCol_ScrollbarBg] = alpha(p.bg, 0.0f);
    c[ImGuiCol_ScrollbarGrab] = mix(p.bg, p.text, 0.25f);
    c[ImGuiCol_ScrollbarGrabHovered] = mix(p.bg, p.text, 0.40f);
    c[ImGuiCol_ScrollbarGrabActive] = accent;
    c[ImGuiCol_CheckMark] = accent;
    c[ImGuiCol_SliderGrab] = accent;
    c[ImGuiCol_SliderGrabActive] = accent;
    c[ImGuiCol_Button] = p.surface;
    c[ImGuiCol_ButtonHovered] = mix(p.surface, accent, 0.25f);
    c[ImGuiCol_ButtonActive] = accent;
    c[ImGuiCol_Header] = accent;                     // selected row
    c[ImGuiCol_HeaderHovered] = alpha(accent, 0.18f);  // hovered row / menu item
    c[ImGuiCol_HeaderActive] = alpha(accent, 0.55f);
    c[ImGuiCol_Separator] = p.border;
    c[ImGuiCol_SeparatorHovered] = accent;
    c[ImGuiCol_SeparatorActive] = accent;
    c[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ResizeGripHovered] = alpha(accent, 0.5f);
    c[ImGuiCol_ResizeGripActive] = accent;
    c[ImGuiCol_TableHeaderBg] = p.surface;
    c[ImGuiCol_TableBorderStrong] = p.border;
    c[ImGuiCol_TableBorderLight] = mix(p.bg, p.border, 0.6f);
    c[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt] = ap.dark ? ImVec4(1, 1, 1, 0.025f) : ImVec4(0, 0, 0, 0.025f);
    c[ImGuiCol_TextSelectedBg] = alpha(accent, 0.35f);
    c[ImGuiCol_NavCursor] = accent;

    style.ScaleAllSizes(scale);
    ImGui::GetStyle() = style;

    g_colors.folderIcon = ImGui::ColorConvertFloat4ToU32(accent);
    g_colors.fileIcon = ImGui::ColorConvertFloat4ToU32(alpha(p.dim, 0.9f));
    g_colors.selectedText = IM_COL32(255, 255, 255, 255);  // GTK puts white text on the accent colour
    g_colors.dimText = p.dim;
}

void loadFonts(const SystemAppearance& ap, float scale) {
    ImGuiIO& io = ImGui::GetIO();
    if (!ap.fontFile.empty()) {
        ImFontConfig cfg;
        cfg.OversampleH = 2;  // sharper sub-pixel horizontal positioning
        cfg.OversampleV = 1;
        const float sizePx = std::round(ap.fontSizePx * scale);
        // Latin + Greek + Cyrillic. Other scripts (CJK, Arabic...) show as '?'
        // until we add fallback fonts; see docs/ROADMAP.md.
        static const ImWchar ranges[] = {0x0020, 0x024F, 0x0370, 0x03FF, 0x0400, 0x052F, 0x2000, 0x206F, 0};
        if (io.Fonts->AddFontFromFileTTF(ap.fontFile.c_str(), sizePx, &cfg, ranges)) return;
    }
    ImFontConfig cfg;
    cfg.SizePixels = std::round(13.0f * scale);
    io.Fonts->AddFontDefault(&cfg);
}

const ThemeColors& themeColors() {
    return g_colors;
}

}  // namespace fe
