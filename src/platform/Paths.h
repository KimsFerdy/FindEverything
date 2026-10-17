#pragma once

#include <string>

namespace fe {

// xdg dirs, created if missing. "" if HOME isn't set or mkdir fails
std::string cacheDirectory();   // $XDG_CACHE_HOME/findeverything  (~/.cache/findeverything)
std::string configDirectory();  // $XDG_CONFIG_HOME/findeverything (~/.config/findeverything)

std::string indexCachePath();    // <cache>/index.bin
std::string settingsFilePath();  // <config>/settings.ini

// from /proc/self/exe
std::string executablePath();

}  // namespace fe
