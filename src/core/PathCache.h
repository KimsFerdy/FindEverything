#pragma once

#include "core/Index.h"

#include <string>
#include <string_view>
#include <vector>

namespace fe {

// Full paths of every *folder*, built once after indexing. Path searches need the
// full path of every record; rebuilding it by walking parents each time would be
// far too slow, but folders are only ~10% of entries, so caching just those is cheap.
// A file's path is then dirPath(parent) + '/' + name.
class PathCache {
public:
    // Requires every folder to appear in the index before its children
    // (true for our scanners, which add a folder before reading it).
    void build(const Index& index);

    std::string_view dirPath(RecordId dir) const { return view(paths_, dir); }
    std::string_view lowerDirPath(RecordId dir) const { return view(lowerPaths_, dir); }

private:
    struct Span {
        uint64_t offset;
        uint32_t len;
    };
    std::string_view view(const std::string& pool, RecordId dir) const;

    std::vector<uint32_t> slot_;  // per record: index into spans_, or kNoSlot for files
    std::vector<Span> spans_;
    std::string paths_;
    std::string lowerPaths_;
};

}  // namespace fe
