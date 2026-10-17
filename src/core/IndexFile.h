#pragma once

#include <cstdint>
#include <string>

#include "core/Index.h"

namespace fe {

// on-disk index: header + raw records + name pool, basically a memory dump.
// not portable across machines, the header checks layout/endianness and we just rescan if it doesn't match.
struct IndexFileInfo {
    int64_t createdAt = 0;  // unix seconds
    std::string source;     // what got scanned, so we notice when settings change
};

// writes <path>.tmp then renames over <path>. rename is atomic so we never end up with half a file
bool saveIndex(const Index& index, const std::string& path, const std::string& source);

// false (and empty index) if missing, old version or corrupt
bool loadIndex(Index& index, const std::string& path, IndexFileInfo* info = nullptr);

}  // namespace fe
