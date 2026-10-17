#include "core/Index.h"

namespace fe {

namespace {

char asciiLower(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

}  // namespace

RecordId Index::add(RecordId parent, std::string_view name, uint8_t flags, uint64_t size, int64_t mtime) {
    FileRecord r{};
    r.parent = parent;
    r.nameOffset = static_cast<uint32_t>(names_.size());
    r.nameLen = static_cast<uint32_t>(name.size());
    r.flags = flags;
    r.size = size;
    r.mtime = mtime;

    names_.append(name);
    for (char c : name) lowerNames_.push_back(asciiLower(c));

    records_.push_back(r);
    return static_cast<RecordId>(records_.size() - 1);
}

void Index::reserve(size_t records, size_t nameBytes) {
    records_.reserve(records);
    names_.reserve(nameBytes);
    lowerNames_.reserve(nameBytes);
}

void Index::clear() {
    records_.clear();
    names_.clear();
    lowerNames_.clear();
}

std::string_view Index::name(RecordId id) const {
    const FileRecord& r = records_[id];
    return std::string_view(names_).substr(r.nameOffset, r.nameLen);
}

std::string_view Index::lowerName(RecordId id) const {
    const FileRecord& r = records_[id];
    return std::string_view(lowerNames_).substr(r.nameOffset, r.nameLen);
}

std::string Index::fullPath(RecordId id) const {
    std::vector<std::string_view> parts;
    for (RecordId cur = id; cur != kNoParent; cur = records_[cur].parent) {
        parts.push_back(name(cur));
    }

    std::string out;
    for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
        // Roots like "/" or "C:\" already end in a separator.
        if (!out.empty() && out.back() != '/' && out.back() != '\\') out += kPathSeparator;
        out += *it;
    }
    return out;
}

}  // namespace fe
