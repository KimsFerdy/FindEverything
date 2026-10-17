#include "platform/Appearance.h"

#include <cstdio>
#include <filesystem>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace fe {

SystemAppearance querySystemAppearance() {
    SystemAppearance ap;
    std::error_code ec;

#if defined(_WIN32)
    DWORD value = 1, size = sizeof value;
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &size) == ERROR_SUCCESS) {
        ap.dark = value == 0;
    }
    DWORD abgr = 0;
    size = sizeof abgr;
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\DWM", L"AccentColor", RRF_RT_REG_DWORD,
                     nullptr, &abgr, &size) == ERROR_SUCCESS) {
        ap.hasAccent = true;
        ap.accent[0] = static_cast<float>(abgr & 0xFF) / 255.0f;
        ap.accent[1] = static_cast<float>((abgr >> 8) & 0xFF) / 255.0f;
        ap.accent[2] = static_cast<float>((abgr >> 16) & 0xFF) / 255.0f;
    }
    if (std::filesystem::exists("C:\\Windows\\Fonts\\segoeui.ttf", ec)) ap.fontFile = "C:\\Windows\\Fonts\\segoeui.ttf";
    ap.fontSizePx = 9.0f * 96.0f / 72.0f;  // Segoe UI 9pt is the Windows UI default

#elif defined(__APPLE__)
    if (FILE* p = popen("defaults read -g AppleInterfaceStyle 2>/dev/null", "r")) {
        char buf[32] = {};
        ap.dark = fgets(buf, sizeof buf, p) != nullptr && buf[0] == 'D';
        pclose(p);
    }
    if (std::filesystem::exists("/System/Library/Fonts/SFNS.ttf", ec)) ap.fontFile = "/System/Library/Fonts/SFNS.ttf";
    ap.fontSizePx = 13.0f;
#endif

    return ap;
}

}  // namespace fe
