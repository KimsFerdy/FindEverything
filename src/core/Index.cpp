#include "core/Index.h"

namespace fe {

namespace {

char asciiLower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }

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

void Index::rebuildLowerNames() {
    lowerNames_.resize(names_.size());
    for (size_t i = 0; i < names_.size(); ++i) lowerNames_[i] = asciiLower(names_[i]);
}

void Index::shrinkToFit() {
    records_.shrink_to_fit();
    names_.shrink_to_fit();
    lowerNames_.shrink_to_fit();
}

void Index::markDeleted(RecordId id) { records_[id].flags |= kIsDeleted; }

void Index::move(RecordId id, RecordId newParent, std::string_view newName) {
    FileRecord& r = records_[id];
    r.parent = newParent;
    if (name(id) == newName) return;
    r.nameOffset = static_cast<uint32_t>(names_.size());
    r.nameLen = static_cast<uint32_t>(newName.size());
    names_.append(newName);
    for (char c : newName) lowerNames_.push_back(asciiLower(c));
}

void Index::setMeta(RecordId id, uint8_t flags, uint64_t size, int64_t mtime) {
    FileRecord& r = records_[id];
    r.flags = flags;
    r.size = size;
    r.mtime = mtime;
}

Index Index::compacted() const {
    // children lists (csr), then dfs from the roots so parents always come first.
    // anything under a deleted folder is dropped with it
    const size_t n = records_.size();
    std::vector<uint32_t> start(n + 1, 0), kids(n);
    std::vector<RecordId> roots;
    for (RecordId id = 0; id < n; ++id) {
        if (records_[id].parent == kNoParent)
            roots.push_back(id);
        else
            ++start[records_[id].parent + 1];
    }
    for (size_t i = 0; i < n; ++i) start[i + 1] += start[i];
    std::vector<uint32_t> fill(start.begin(), start.end() - 1);
    for (RecordId id = 0; id < n; ++id) {
        if (records_[id].parent != kNoParent) kids[fill[records_[id].parent]++] = id;
    }

    Index out;
    out.reserve(n, names_.size());
    std::vector<std::pair<RecordId, RecordId>> stack;  // old id, new parent id
    for (auto it = roots.rbegin(); it != roots.rend(); ++it) stack.emplace_back(*it, kNoParent);
    while (!stack.empty()) {
        auto [old, newParent] = stack.back();
        stack.pop_back();
        const FileRecord& r = records_[old];
        if (r.flags & kIsDeleted) continue;
        const RecordId id = out.add(newParent, name(old), r.flags, r.size, r.mtime);
        for (uint32_t k = start[old + 1]; k > start[old]; --k) stack.emplace_back(kids[k - 1], id);
    }
    return out;
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
        if (!out.empty() && out.back() != kPathSeparator) out += kPathSeparator;  // "/" already ends in one
        out += *it;
    }
    return out;
}

}  // namespace fe
