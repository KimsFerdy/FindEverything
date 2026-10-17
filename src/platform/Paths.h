#pragma once

#include <string>

namespace fe {

// Per-user folder for data we can always regenerate (the index), created if needed:
//   Linux    $XDG_CACHE_HOME/findeverything  (default ~/.cache/findeverything)
//   Windows  %LOCALAPPDATA%\FindEverything
//   macOS    ~/Library/Caches/FindEverything
// Empty string if it cannot be determined or created.
std::string cacheDirectory();

std::string indexCachePath();  // cacheDirectory() + "/index.bin"

}  // namespace fe
