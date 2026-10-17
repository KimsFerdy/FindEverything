#include "core/PathCache.h"

#include <cctype>

namespace fe {

namespace {

constexpr uint32_t kNoSlot = UINT32_MAX;

}  // namespace

void PathCache::build(const Index& index) {
    slot_.assign(index.size(), kNoSlot);
    spans_.clear();
    paths_.clear();
    lowerPaths_.clear();

    std::string path;
    for (RecordId id = 0; id < index.size(); ++id) {
        const FileRecord& r = index.record(id);
        if (!(r.flags & kIsDir)) continue;

        if (r.parent == kNoParent || slot_[r.parent] == kNoSlot) {
            path = index.fullPath(id);  // roots (and the unexpected out-of-order case)
        } else {
            path.assign(dirPath(r.parent));
            if (path.back() != '/' && path.back() != '\\') path += kPathSeparator;
            path.append(index.name(id));
        }

        slot_[id] = static_cast<uint32_t>(spans_.size());
        spans_.push_back({paths_.size(), static_cast<uint32_t>(path.size())});
        paths_.append(path);
        for (char c : path) lowerPaths_.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
}

std::string_view PathCache::view(const std::string& pool, RecordId dir) const {
    if (dir >= slot_.size() || slot_[dir] == kNoSlot) return {};
    const Span& s = spans_[slot_[dir]];
    return std::string_view(pool).substr(s.offset, s.len);
}

}  // namespace fe
