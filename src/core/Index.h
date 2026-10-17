#pragma once

#include "core/FileRecord.h"

#include <string>
#include <string_view>
#include <vector>

namespace fe {

// In-memory database of every file and folder.
//
// Layout: one flat vector of FileRecord, plus one big string ("name pool") holding
// all names back to back. A lowercased copy of the pool makes case-insensitive
// search a plain memory scan, which is what keeps searching millions of names fast.
class Index {
public:
    RecordId add(RecordId parent, std::string_view name, uint8_t flags, uint64_t size, int64_t mtime);

    void reserve(size_t records, size_t nameBytes);
    void clear();

    size_t size() const { return records_.size(); }
    const FileRecord& record(RecordId id) const { return records_[id]; }

    std::string_view name(RecordId id) const;
    std::string_view lowerName(RecordId id) const;

    // Rebuilds "/home/kims/notes.txt" by walking parent links up to the root.
    std::string fullPath(RecordId id) const;

private:
    std::vector<FileRecord> records_;
    std::string names_;
    std::string lowerNames_;  // TODO: ASCII-only lowering; full UTF-8 case folding later
};

}  // namespace fe
