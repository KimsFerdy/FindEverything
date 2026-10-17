#pragma once

#include "core/Index.h"

#include <cstdint>
#include <string>

namespace fe {

// Binary index file: a header followed by the raw records and the name pool,
// exactly as they sit in memory, so loading is one big sequential read.
//
// Not portable between machines with different endianness or struct layout;
// the header records both, and a mismatching file is simply rejected (we rescan).
struct IndexFileInfo {
    int64_t createdAt = 0;  // unix seconds when the scan finished
    std::string source;     // what was scanned (roots), to detect a config change
};

// Writes to "<path>.tmp", then renames over <path>. rename() is atomic on Linux,
// so readers see either the old file or the new one, never a half-written one.
bool saveIndex(const Index& index, const std::string& path, const std::string& source);

// Returns false (leaving `index` empty) if the file is missing, from another
// version, or corrupt.
bool loadIndex(Index& index, const std::string& path, IndexFileInfo* info = nullptr);

}  // namespace fe
