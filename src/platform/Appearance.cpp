// dark mode + accent from the xdg desktop portal (gnome, kde, cinnamon all implement it),
// font + icon theme from gsettings, font file from fc-match.
// shelling out to gdbus/gsettings is a bit lazy but avoids linking glib.
// TODO(claude): talk dbus directly (sd-bus?) and listen for SettingChanged so theme switches live

#include "platform/Appearance.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "platform/Process.h"

namespace fe {

namespace {

std::string trim(std::string s) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

std::string toLower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// gsettings prints strings quoted: 'Ubuntu 10'
std::string gsettingsGet(const char* schema, const char* key) {
    std::string out;
    if (posix::runCapture({"gsettings", "get", schema, key}, &out) != 0) return {};
    out = trim(out);
    if (out.size() >= 2 && out.front() == '\'' && out.back() == '\'') out = out.substr(1, out.size() - 2);
    return out;
}

std::string portalRead(const char* key) {
    std::string out;
    if (posix::runCapture({"gdbus", "call", "--session", "--dest", "org.freedesktop.portal.Desktop", "--object-path",
                           "/org/freedesktop/portal/desktop", "--method", "org.freedesktop.portal.Settings.ReadOne",
                           "org.freedesktop.appearance", key},
                          &out) != 0) {
        return {};
    }
    return out;
}

// every number in e.g. "(<(0.12, 0.61, 0.87)>,)"
std::vector<double> parseNumbers(const std::string& s) {
    std::vector<double> nums;
    const char* p = s.c_str();
    while (*p) {
        if (std::isdigit(static_cast<unsigned char>(*p)) ||
            (*p == '-' && std::isdigit(static_cast<unsigned char>(p[1])))) {
            char* end = nullptr;
            nums.push_back(std::strtod(p, &end));
            p = end;
        } else {
            ++p;
        }
    }
    return nums;
}

// each desktop keeps its own copy of these keys
const char* interfaceSchema() {
    const char* de = std::getenv("XDG_CURRENT_DESKTOP");
    std::string desktop = de ? toLower(de) : "";
    if (desktop.find("cinnamon") != std::string::npos) return "org.cinnamon.desktop.interface";
    if (desktop.find("mate") != std::string::npos) return "org.mate.interface";
    return "org.gnome.desktop.interface";
}

}  // namespace

SystemAppearance querySystemAppearance() {
    SystemAppearance ap;
    const char* schema = interfaceSchema();

    // 0 = no preference, 1 = dark, 2 = light
    std::string scheme = portalRead("color-scheme");
    size_t u = scheme.find("uint32 ");
    int preference = u != std::string::npos ? std::atoi(scheme.c_str() + u + 7) : 0;
    if (preference != 0) {
        ap.dark = preference == 1;
    } else {  // no answer, guess from the gtk theme name ("Mint-Y-Dark-Aqua")
        ap.dark = gsettingsGet("org.gnome.desktop.interface", "color-scheme") == "prefer-dark" ||
                  toLower(gsettingsGet(schema, "gtk-theme")).find("dark") != std::string::npos;
    }

    // values outside 0..1 mean "not set"
    std::vector<double> rgb = parseNumbers(portalRead("accent-color"));
    if (rgb.size() == 3 && std::all_of(rgb.begin(), rgb.end(), [](double v) { return v >= 0.0 && v <= 1.0; })) {
        ap.hasAccent = true;
        for (int i = 0; i < 3; ++i) ap.accent[i] = static_cast<float>(rgb[i]);
    }

    // "Ubuntu 10" -> family Ubuntu, 10pt
    std::string fontName = gsettingsGet(schema, "font-name");
    std::string family = "sans-serif";
    float sizePt = 10.0f;
    size_t space = fontName.find_last_of(' ');
    if (space != std::string::npos && std::isdigit(static_cast<unsigned char>(fontName[space + 1]))) {
        family = fontName.substr(0, space);
        sizePt = std::strtof(fontName.c_str() + space + 1, nullptr);
    } else if (!fontName.empty()) {
        family = fontName;
    }

    // truetype only, imgui's rasterizer can't do cff/otf outlines
    std::string file;
    if (posix::runCapture({"fc-match", "-f", "%{file}", family + ":fontformat=TrueType"}, &file) == 0) {
        file = trim(file);
        std::string ext = toLower(file.substr(file.find_last_of('.') + 1));
        if (ext == "ttf" || ext == "ttc") ap.fontFile = file;
    }

    // pt -> px at 96 dpi
    float textScale = 1.0f;
    std::string scaleStr = gsettingsGet(schema, "text-scaling-factor");
    if (!scaleStr.empty()) textScale = std::max(0.5f, std::strtof(scaleStr.c_str(), nullptr));
    ap.fontSizePx = sizePt * 96.0f / 72.0f * textScale;

    ap.iconTheme = gsettingsGet(schema, "icon-theme");

    return ap;
}

}  // namespace fe
