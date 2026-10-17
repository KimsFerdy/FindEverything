#pragma once

#include <cstdint>

namespace fe {

// Index position of a file/folder. 32 bits = up to ~4 billion entries.
using RecordId = uint32_t;
inline constexpr RecordId kNoParent = UINT32_MAX;

enum RecordFlags : uint8_t {
    kIsDir     = 1 << 0,
    kIsSymlink = 1 << 1,
};

// One entry in the index. Kept small on purpose: millions of these live in RAM.
// We store only the *name* plus a parent link, never the full path — the full
// path is rebuilt on demand by walking parents (this is how Everything does it too).
struct FileRecord {
    RecordId parent;      // kNoParent for a root ("/", "C:\")
    uint32_t nameOffset;  // into Index's name pool
    uint32_t nameLen;
    uint8_t  flags;       // RecordFlags
    uint64_t size;        // bytes (0 for folders)
    int64_t  mtime;       // seconds since Unix epoch
};

}  // namespace fe
