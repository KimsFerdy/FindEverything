// Linux filesystem scanner, using raw POSIX calls instead of std::filesystem so
// you can see what actually happens underneath:
//
//   open(O_DIRECTORY)  get a file descriptor for a directory
//   readdir()          list its entries (wraps the getdents64 syscall)
//   fstatat()          size / mtime / type of one entry, relative to the dir fd
//
// Try `strace -c ./fe-cli --root ~ foo` to count the syscalls this makes.

#include "platform/Scanner.h"
#include "platform/linux/LinuxMounts.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <unordered_set>

namespace fe {

namespace {

struct PendingDir {
    std::string path;
    RecordId id;
};

bool isDotOrDotDot(const char* n) {
    return n[0] == '.' && (n[1] == '\0' || (n[1] == '.' && n[2] == '\0'));
}

}  // namespace

bool scanFilesystem(Index& index, const ScanOptions& opts, ScanProgress* progress) {
    const std::unordered_set<std::string> excluded(opts.excludes.begin(), opts.excludes.end());

    // Iterative depth-first walk with an explicit stack: deep trees can't overflow
    // the call stack the way recursion could.
    std::vector<PendingDir> stack;
    for (const std::string& root : opts.roots) {
        struct stat st {};
        if (lstat(root.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        RecordId id = index.add(kNoParent, root, kIsDir, 0, st.st_mtime);
        stack.push_back({root, id});
    }

    while (!stack.empty()) {
        if (progress && progress->cancel.load(std::memory_order_relaxed)) return false;

        PendingDir dir = std::move(stack.back());
        stack.pop_back();

        // O_NOFOLLOW: never follow a symlink into another directory (avoids loops).
        int fd = open(dir.path.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (fd < 0) {
            if (progress) progress->errors.fetch_add(1, std::memory_order_relaxed);
            continue;
        }
        DIR* d = fdopendir(fd);  // takes ownership of fd
        if (!d) {
            close(fd);
            continue;
        }

        while (dirent* e = readdir(d)) {
            const char* name = e->d_name;
            if (isDotOrDotDot(name)) continue;

            // d_type comes for free from getdents64 on most filesystems. Some
            // (older XFS, some network FS) report DT_UNKNOWN and need a stat().
            unsigned char type = e->d_type;
            uint64_t size = 0;
            int64_t mtime = 0;
            if (opts.collectMetadata || type == DT_UNKNOWN) {
                struct stat st {};
                if (fstatat(fd, name, &st, AT_SYMLINK_NOFOLLOW) == 0) {
                    size = static_cast<uint64_t>(st.st_size);
                    mtime = st.st_mtime;
                    if (S_ISDIR(st.st_mode)) type = DT_DIR;
                    else if (S_ISLNK(st.st_mode)) type = DT_LNK;
                    else if (type == DT_UNKNOWN) type = DT_REG;
                }
            }

            uint8_t flags = 0;
            if (type == DT_DIR) flags |= kIsDir;
            if (type == DT_LNK) flags |= kIsSymlink;

            RecordId id = index.add(dir.id, name, flags, (flags & kIsDir) ? 0 : size, mtime);
            if (progress) progress->entries.fetch_add(1, std::memory_order_relaxed);

            if (type == DT_DIR) {
                std::string child = dir.path;
                if (child.back() != '/') child += '/';
                child += name;
                if (!excluded.count(child)) stack.push_back({std::move(child), id});
            }
        }
        closedir(d);  // also closes fd
    }
    return true;
}

std::vector<std::string> defaultRoots() {
    return {"/"};
}

std::vector<std::string> defaultExcludes() {
    // Skip every mount point that is a kernel/virtual filesystem (/proc, /sys, /dev,
    // /run, snap squashfs images...). Real disks mounted anywhere are still scanned.
    std::vector<std::string> out = {"/proc", "/sys", "/dev"};
    for (const linux_fs::MountInfo& m : linux_fs::readMounts()) {
        if (linux_fs::isPseudoFilesystem(m.fsType) && m.mountPoint != "/") out.push_back(m.mountPoint);
    }
    return out;
}

}  // namespace fe
