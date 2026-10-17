// Portable scanner built on std::filesystem. Used on Windows and macOS until their
// native backends exist (NTFS MFT reading on Windows is what makes Everything
// index a drive in seconds). Slower, but it works everywhere.

#include "platform/Scanner.h"

#include <chrono>
#include <filesystem>
#include <system_error>

namespace fe {

namespace fs = std::filesystem;

namespace {

std::string toUtf8(const fs::path& p) {
    std::u8string s = p.u8string();
    return std::string(reinterpret_cast<const char*>(s.data()), s.size());
}

int64_t toUnixSeconds(fs::file_time_type t) {
    using namespace std::chrono;
    auto sys = time_point_cast<seconds>(t - fs::file_time_type::clock::now() + system_clock::now());
    return sys.time_since_epoch().count();
}

}  // namespace

bool scanFilesystem(Index& index, const ScanOptions& opts, ScanProgress* progress) {
    std::vector<fs::path> excluded(opts.excludes.begin(), opts.excludes.end());
    auto isExcluded = [&](const fs::path& p) {
        for (const fs::path& ex : excluded) {
            if (p == ex) return true;
        }
        return false;
    };

    std::vector<std::pair<fs::path, RecordId>> stack;
    for (const std::string& root : opts.roots) {
        std::error_code ec;
        if (!fs::is_directory(root, ec)) continue;
        stack.emplace_back(fs::path(root), index.add(kNoParent, root, kIsDir, 0, 0));
    }

    while (!stack.empty()) {
        if (progress && progress->cancel.load(std::memory_order_relaxed)) return false;

        auto [dirPath, dirId] = std::move(stack.back());
        stack.pop_back();

        std::error_code ec;
        fs::directory_iterator it(dirPath, fs::directory_options::skip_permission_denied, ec);
        if (ec) {
            if (progress) progress->errors.fetch_add(1, std::memory_order_relaxed);
            continue;
        }

        for (; it != fs::directory_iterator(); it.increment(ec)) {
            if (ec) break;
            const fs::directory_entry& entry = *it;

            const bool isLink = entry.is_symlink(ec);
            const bool isDir = !isLink && entry.is_directory(ec);
            uint8_t flags = (isDir ? kIsDir : 0) | (isLink ? kIsSymlink : 0);

            uint64_t size = 0;
            int64_t mtime = 0;
            if (opts.collectMetadata) {
                if (!isDir && !isLink) size = entry.file_size(ec);
                auto t = entry.last_write_time(ec);
                if (!ec) mtime = toUnixSeconds(t);
            }

            RecordId id = index.add(dirId, toUtf8(entry.path().filename()), flags, size, mtime);
            if (progress) progress->entries.fetch_add(1, std::memory_order_relaxed);

            if (isDir && !isExcluded(entry.path())) stack.emplace_back(entry.path(), id);
        }
    }
    return true;
}

std::vector<std::string> defaultRoots() {
#ifdef _WIN32
    std::vector<std::string> roots;
    for (char drive = 'A'; drive <= 'Z'; ++drive) {
        std::string root = std::string(1, drive) + ":\\";
        std::error_code ec;
        if (fs::exists(root, ec)) roots.push_back(root);
    }
    return roots;
#else
    return {"/"};
#endif
}

std::vector<std::string> defaultExcludes() {
#if defined(__APPLE__)
    // /System/Volumes/Data is the same disk as "/" seen again via firmlinks.
    return {"/dev", "/System/Volumes", "/private/var/vm"};
#else
    return {};
#endif
}

}  // namespace fe
