// plain posix instead of std::filesystem: open(O_DIRECTORY) + readdir (getdents64) + fstatat.
// way less overhead and we get d_type / st_dev for free

#include "platform/Scanner.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <unordered_set>

#include "platform/Mounts.h"

namespace fe {

namespace {

struct PendingDir {
    std::string path;
    RecordId id;
    dev_t device;
};

bool isDotOrDotDot(const char* n) { return n[0] == '.' && (n[1] == '\0' || (n[1] == '.' && n[2] == '\0')); }

}  // namespace

bool scanFilesystem(Index& index, const ScanOptions& opts, ScanProgress* progress) {
    const std::unordered_set<std::string> excluded(opts.excludes.begin(), opts.excludes.end());
    const std::unordered_set<std::string> rootSet(opts.roots.begin(), opts.roots.end());

    // explicit stack instead of recursion, deep trees won't blow the call stack
    std::vector<PendingDir> stack;
    for (const std::string& root : opts.roots) {
        struct stat st {};
        if (lstat(root.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        RecordId id = index.add(kNoParent, root, kIsDir, 0, st.st_mtime);
        stack.push_back({root, id, st.st_dev});
    }

    while (!stack.empty()) {
        if (progress && progress->cancel.load(std::memory_order_relaxed)) return false;

        PendingDir dir = std::move(stack.back());
        stack.pop_back();

        // O_NOFOLLOW so symlinked dirs can't send us in loops
        int fd = open(dir.path.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (fd < 0) {
            if (progress) progress->errors.fetch_add(1, std::memory_order_relaxed);
            continue;
        }
        DIR* d = fdopendir(fd);  // owns fd now
        if (!d) {
            close(fd);
            continue;
        }

        while (dirent* e = readdir(d)) {
            const char* name = e->d_name;
            if (isDotOrDotDot(name)) continue;

            // some filesystems give DT_UNKNOWN, those need a stat
            unsigned char type = e->d_type;
            uint64_t size = 0;
            int64_t mtime = 0;
            dev_t device = dir.device;
            bool executable = false;
            const bool needStat =
                opts.collectMetadata || type == DT_UNKNOWN || (opts.stayOnFilesystem && type == DT_DIR);
            if (needStat) {
                struct stat st {};
                if (fstatat(fd, name, &st, AT_SYMLINK_NOFOLLOW) == 0) {
                    size = static_cast<uint64_t>(st.st_size);
                    mtime = st.st_mtime;
                    device = st.st_dev;
                    executable = S_ISREG(st.st_mode) && (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH));
                    if (S_ISDIR(st.st_mode))
                        type = DT_DIR;
                    else if (S_ISLNK(st.st_mode))
                        type = DT_LNK;
                    else if (type == DT_UNKNOWN)
                        type = DT_REG;
                }
            }

            std::string child;
            bool descend = false;
            if (type == DT_DIR) {
                child = dir.path;
                if (child.back() != '/') child += '/';
                child += name;
                if (excluded.count(child)) continue;
                // st_dev changes = mount point, we're about to walk onto another disk
                const bool otherFilesystem = device != dir.device;
                if (otherFilesystem && rootSet.count(child)) continue;  // it's its own root
                descend = !(opts.stayOnFilesystem && otherFilesystem);
            }

            uint8_t flags = 0;
            if (type == DT_DIR) flags |= kIsDir;
            if (type == DT_LNK) flags |= kIsSymlink;
            if (executable) flags |= kIsExecutable;

            RecordId id = index.add(dir.id, name, flags, (flags & kIsDir) ? 0 : size, mtime);
            if (progress) progress->entries.fetch_add(1, std::memory_order_relaxed);

            if (descend) stack.push_back({std::move(child), id, device});
        }
        closedir(d);  // closes fd too
    }
    return true;
}

std::vector<std::string> defaultExcludes() {
    std::vector<std::string> out = {"/proc", "/sys", "/dev"};
    for (const MountInfo& m : readMounts()) {
        if (isPseudoFilesystem(m.fsType) && m.mountPoint != "/") out.push_back(m.mountPoint);
    }
    return out;
}

}  // namespace fe
