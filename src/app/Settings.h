#pragma once

#include <string>
#include <vector>

#include "platform/Scanner.h"

namespace fe {

struct WindowGeometry {
    int x = -1, y = -1;  // -1 = let the window manager pick
    int width = 1100, height = 700;
    bool maximized = false;

    bool operator==(const WindowGeometry&) const = default;
};

// ~/.config/findeverything/settings.ini, plain key = value so you can edit it by hand
struct Settings {
    enum class Theme { System, Dark, Light };

    bool launchAtLogin = false;  // not in the ini, mirrors the autostart file
    bool startInTray = true;     // at login: no window, just the tray icon
    bool trayIcon = true;
    bool closeToTray = true;  // closing the window keeps us running in the tray
    bool rescanOnStartup = true;
    bool liveUpdates = true;  // inotify, keeps the index current while running

    std::vector<std::string> excludedVolumes;  // mount points that got unticked
    std::vector<std::string> excludedFolders;

    Theme theme = Theme::System;
    bool customAccent = false;
    float accent[3] = {0.12f, 0.62f, 0.87f};
    float fontZoom = 1.0f;

    WindowGeometry window;

    bool operator==(const Settings&) const = default;
};

Settings loadSettings();
bool saveSettings(const Settings& settings);

// every disk that isn't unticked, minus excluded folders
ScanOptions scanOptionsFor(const Settings& settings);
// stored in the index cache so a cache built with other options gets ignored. gui + cli share it
std::string scanSourceFor(const ScanOptions& options, const Settings& settings);

}  // namespace fe
