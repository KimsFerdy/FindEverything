#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "core/Index.h"

namespace fe {

// full path of every folder, built once per index.
// path search needs everyone's full path and walking parents for 500k records is way too slow.
// folders are ~10% of entries so caching only those is cheap. file path = dirPath(parent) + '/' + name
class PathCache {
public:
    // needs folders to come before their children in the index (our scanner does that)
    void build(const Index& index);

    std::string_view dirPath(RecordId dir) const { return view(paths_, dir); }
    std::string_view lowerDirPath(RecordId dir) const { return view(lowerPaths_, dir); }

private:
    struct Span {
        uint64_t offset;
        uint32_t len;
    };
    std::string_view view(const std::string& pool, RecordId dir) const;

    std::vector<uint32_t> slot_;  // per record, index into spans_ (or none for files)
    std::vector<Span> spans_;
    std::string paths_;
    std::string lowerPaths_;
};

}  // namespace fe
