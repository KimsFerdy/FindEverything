#pragma once

#include <string>

namespace fe {

// Per-user folder for data we can always regenerate (the index), created if needed:
//   Linux    $XDG_CACHE_HOME/findeverything  (default ~/.cache/findeverything)
//   Windows  %LOCALAPPDATA%\FindEverything
//   macOS    ~/Library/Caches/FindEverything
// Empty string if it cannot be determined or created.
std::string cacheDirectory();

// Per-user folder for settings, created if needed:
//   Linux    $XDG_CONFIG_HOME/findeverything  (default ~/.config/findeverything)
//   Windows  %APPDATA%\FindEverything
//   macOS    ~/Library/Application Support/FindEverything
std::string configDirectory();

std::string indexCachePath();     // cacheDirectory() + "/index.bin"
std::string settingsFilePath();   // configDirectory() + "/settings.ini"

// Absolute path of the running executable ("" if unknown).
std::string executablePath();

}  // namespace fe
