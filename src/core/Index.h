#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "core/FileRecord.h"

namespace fe {

inline constexpr char kPathSeparator = '/';

struct IndexFileInfo;

// every file and folder we know about.
// flat records + one big string with all the names back to back. there's also a
// lowercased copy of that string so case-insensitive search is just a memory scan.
class Index {
public:
    RecordId add(RecordId parent, std::string_view name, uint8_t flags, uint64_t size, int64_t mtime);

    void reserve(size_t records, size_t nameBytes);
    void clear();
    void shrinkToFit();  // vectors grow 2x while scanning, give the slack back

    // live updates. ids stay stable, deleted records just get a tombstone
    void markDeleted(RecordId id);
    void move(RecordId id, RecordId newParent, std::string_view newName);  // old name stays in the pool as garbage
    void setMeta(RecordId id, uint8_t flags, uint64_t size, int64_t mtime);
    bool isDeleted(RecordId id) const { return records_[id].flags & kIsDeleted; }

    // copy without tombstones, parents before children (what the index file and PathCache want)
    Index compacted() const;

    size_t size() const { return records_.size(); }
    const FileRecord& record(RecordId id) const { return records_[id]; }

    std::string_view name(RecordId id) const;
    std::string_view lowerName(RecordId id) const;

    // walks the parents up to the root. fine for a few rows, don't call it in a hot loop
    std::string fullPath(RecordId id) const;

private:
    friend bool saveIndex(const Index&, const std::string&, const std::string&);
    friend bool loadIndex(Index&, const std::string&, IndexFileInfo*);
    void rebuildLowerNames();

    std::vector<FileRecord> records_;
    std::string names_;
    std::string lowerNames_;  // TODO(claude): ascii only for now, proper utf-8 case folding later
};

}  // namespace fe
