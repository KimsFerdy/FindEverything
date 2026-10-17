#include "app/Settings.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <system_error>

#include "platform/Autostart.h"
#include "platform/Paths.h"
#include "platform/Volumes.h"

namespace fe {

namespace {

std::string trim(const std::string& s) {
    const size_t b = s.find_first_not_of(" \t\r");
    const size_t e = s.find_last_not_of(" \t\r");
    return b == std::string::npos ? std::string() : s.substr(b, e - b + 1);
}

bool parseBool(const std::string& v) { return v == "true" || v == "1" || v == "yes"; }

}  // namespace

ScanOptions scanOptionsFor(const Settings& settings) {
    ScanOptions opts;
    const auto& skip = settings.excludedVolumes;
    for (const Volume& v : listVolumes()) {
        if (std::find(skip.begin(), skip.end(), v.path) == skip.end()) opts.roots.push_back(v.path);
    }
    opts.excludes = defaultExcludes();
    opts.excludes.insert(opts.excludes.end(), settings.excludedFolders.begin(), settings.excludedFolders.end());
    opts.stayOnFilesystem = true;  // each disk is its own root
    return opts;
}

std::string scanSourceFor(const ScanOptions& options, const Settings& settings) {
    std::string out;
    for (const std::string& r : options.roots) out += r + '\n';
    out += "exclude:\n";
    for (const std::string& f : settings.excludedFolders) out += f + '\n';
    return out;
}

Settings loadSettings() {
    Settings s;
    s.launchAtLogin = isLaunchAtLoginEnabled();

    // sections are just for humans, keys are unique anyway
    std::ifstream in(settingsFilePath());
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';' || line[0] == '[') continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = trim(line.substr(0, eq));
        const std::string value = trim(line.substr(eq + 1));

        if (key == "rescan_on_startup")
            s.rescanOnStartup = parseBool(value);
        else if (key == "live_updates")
            s.liveUpdates = parseBool(value);
        else if (key == "start_in_tray")
            s.startInTray = parseBool(value);
        else if (key == "tray_icon")
            s.trayIcon = parseBool(value);
        else if (key == "close_to_tray")
            s.closeToTray = parseBool(value);
        else if (key == "exclude_volume")
            s.excludedVolumes.push_back(value);
        else if (key == "exclude_folder")
            s.excludedFolders.push_back(value);
        else if (key == "theme")
            s.theme = value == "dark"    ? Settings::Theme::Dark
                      : value == "light" ? Settings::Theme::Light
                                         : Settings::Theme::System;
        else if (key == "custom_accent")
            s.customAccent = parseBool(value);
        else if (key == "accent")
            std::sscanf(value.c_str(), "%f %f %f", &s.accent[0], &s.accent[1], &s.accent[2]);
        else if (key == "font_zoom")
            s.fontZoom = std::clamp(std::strtof(value.c_str(), nullptr), 0.7f, 2.0f);
        else if (key == "window")
            std::sscanf(value.c_str(), "%d %d %d %d", &s.window.x, &s.window.y, &s.window.width, &s.window.height);
        else if (key == "window_maximized")
            s.window.maximized = parseBool(value);
    }
    return s;
}

bool saveSettings(const Settings& s) {
    const std::string path = settingsFilePath();
    if (path.empty()) return false;

    const char* theme = s.theme == Settings::Theme::Dark    ? "dark"
                        : s.theme == Settings::Theme::Light ? "light"
                                                            : "system";
    {
        std::ofstream out(path + ".tmp", std::ios::trunc);
        out << "# FindEverything settings. fine to edit while the app is closed\n\n"
            << "[general]\n"
            << "rescan_on_startup = " << (s.rescanOnStartup ? "true" : "false") << "\n"
            << "live_updates = " << (s.liveUpdates ? "true" : "false") << "\n"
            << "start_in_tray = " << (s.startInTray ? "true" : "false") << "\n"
            << "tray_icon = " << (s.trayIcon ? "true" : "false") << "\n"
            << "close_to_tray = " << (s.closeToTray ? "true" : "false") << "\n\n"
            << "[indexes]\n";
        for (const std::string& v : s.excludedVolumes) out << "exclude_volume = " << v << "\n";
        for (const std::string& f : s.excludedFolders) out << "exclude_folder = " << f << "\n";
        out << "\n[appearance]\n"
            << "theme = " << theme << "\n"
            << "custom_accent = " << (s.customAccent ? "true" : "false") << "\n"
            << "accent = " << s.accent[0] << " " << s.accent[1] << " " << s.accent[2] << "\n"
            << "font_zoom = " << s.fontZoom << "\n\n"
            << "[window]\n"
            << "window = " << s.window.x << " " << s.window.y << " " << s.window.width << " " << s.window.height << "\n"
            << "window_maximized = " << (s.window.maximized ? "true" : "false") << "\n";
        if (!out.flush()) return false;
    }
    std::error_code ec;
    std::filesystem::rename(path + ".tmp", path, ec);  // same tmp + rename trick as the index
    return !ec;
}

}  // namespace fe
