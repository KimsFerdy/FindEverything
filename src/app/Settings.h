#pragma once

#include "platform/Scanner.h"

#include <string>
#include <vector>

namespace fe {

// User preferences, stored as a small INI-style text file
// (~/.config/findeverything/settings.ini). Plain text on purpose: you can read
// and edit it by hand, and unknown keys are ignored.
struct Settings {
    enum class Theme { System, Dark, Light };

    // General
    bool launchAtLogin = false;   // mirrored from the OS autostart entry, not stored
    bool rescanOnStartup = true;  // false: trust the saved index until F5

    // Indexes
    std::vector<std::string> excludedVolumes;  // mount points the user unticked
    std::vector<std::string> excludedFolders;  // absolute paths left out of the index

    // Appearance
    Theme theme = Theme::System;
    bool customAccent = false;
    float accent[3] = {0.12f, 0.62f, 0.87f};
    float fontZoom = 1.0f;

    bool operator==(const Settings&) const = default;
};

Settings loadSettings();  // defaults for anything missing
bool saveSettings(const Settings& settings);

// What to scan given the settings: every disk not unticked, minus excluded folders.
ScanOptions scanOptionsFor(const Settings& settings);
// A description of those options, stored in the index cache: a cache built
// with different options is ignored. Shared by the GUI and fe-cli.
std::string scanSourceFor(const ScanOptions& options, const Settings& settings);

}  // namespace fe
