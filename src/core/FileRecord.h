#pragma once

#include <cstdint>

namespace fe {

using RecordId = uint32_t;
inline constexpr RecordId kNoParent = UINT32_MAX;

enum RecordFlags : uint8_t {
    kIsDir = 1 << 0,
    kIsSymlink = 1 << 1,
    kIsExecutable = 1 << 2,
};

// keep this small, there's millions of them in ram.
// no full paths stored, just name + parent. paths get rebuilt when needed (same trick Everything uses)
struct FileRecord {
    RecordId parent;      // kNoParent for roots
    uint32_t nameOffset;  // into the name pool
    uint32_t nameLen;
    uint8_t flags;
    uint64_t size;
    int64_t mtime;  // unix seconds
};

}  // namespace fe
