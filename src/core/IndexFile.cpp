#include "core/IndexFile.h"

#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace fe {

namespace {

constexpr char kMagic[8] = {'F', 'E', 'I', 'N', 'D', 'E', 'X', '\0'};
constexpr uint32_t kVersion = 1;
constexpr uint32_t kByteOrderMark = 0x01020304;

struct FileHeader {
    char magic[8];
    uint32_t version;
    uint32_t byteOrder;
    uint32_t recordSize;
    uint32_t sourceLen;
    uint64_t recordCount;
    uint64_t nameBytes;
    int64_t createdAt;
};

}  // namespace

bool saveIndex(const Index& index, const std::string& path, const std::string& source) {
    FileHeader h{};
    std::memcpy(h.magic, kMagic, sizeof kMagic);
    h.version = kVersion;
    h.byteOrder = kByteOrderMark;
    h.recordSize = sizeof(FileRecord);
    h.sourceLen = static_cast<uint32_t>(source.size());
    h.recordCount = index.records_.size();
    h.nameBytes = index.names_.size();
    h.createdAt =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    const std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out.write(reinterpret_cast<const char*>(&h), sizeof h);
        out.write(source.data(), static_cast<std::streamsize>(source.size()));
        out.write(reinterpret_cast<const char*>(index.records_.data()),
                  static_cast<std::streamsize>(index.records_.size() * sizeof(FileRecord)));
        out.write(index.names_.data(), static_cast<std::streamsize>(index.names_.size()));
        if (!out.flush()) {
            std::error_code ec;
            std::filesystem::remove(tmp, ec);
            return false;
        }
    }

    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    if (ec) std::filesystem::remove(tmp, ec);
    return !ec;
}

bool loadIndex(Index& index, const std::string& path, IndexFileInfo* info) {
    index.clear();

    std::ifstream in(path, std::ios::binary);
    if (!in) return false;

    FileHeader h{};
    if (!in.read(reinterpret_cast<char*>(&h), sizeof h)) return false;
    if (std::memcmp(h.magic, kMagic, sizeof kMagic) != 0 || h.version != kVersion || h.byteOrder != kByteOrderMark ||
        h.recordSize != sizeof(FileRecord) || h.recordCount >= kNoParent) {
        return false;
    }

    // check the size before allocating anything off of header values
    std::error_code ec;
    const uint64_t expected = sizeof h + h.sourceLen + h.recordCount * sizeof(FileRecord) + h.nameBytes;
    if (std::filesystem::file_size(path, ec) != expected || ec) return false;

    std::string source(h.sourceLen, '\0');
    index.records_.resize(h.recordCount);
    index.names_.resize(h.nameBytes);
    in.read(source.data(), static_cast<std::streamsize>(source.size()));
    in.read(reinterpret_cast<char*>(index.records_.data()),
            static_cast<std::streamsize>(h.recordCount * sizeof(FileRecord)));
    in.read(index.names_.data(), static_cast<std::streamsize>(h.nameBytes));
    if (!in) {
        index.clear();
        return false;
    }

    // don't trust the file. bad offsets = out of bounds reads later.
    // parents also have to come before children, PathCache depends on that
    for (RecordId id = 0; id < index.records_.size(); ++id) {
        const FileRecord& r = index.records_[id];
        const bool nameOk = uint64_t{r.nameOffset} + r.nameLen <= h.nameBytes;
        const bool parentOk = r.parent == kNoParent || r.parent < id;
        if (!nameOk || !parentOk) {
            index.clear();
            return false;
        }
    }

    index.rebuildLowerNames();
    if (info) {
        info->createdAt = h.createdAt;
        info->source = std::move(source);
    }
    return true;
}

}  // namespace fe
