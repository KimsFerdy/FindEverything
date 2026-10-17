#include "platform/Trash.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <ctime>

namespace fe {

namespace {

bool fail(std::string* error, const std::string& what) {
    if (error) *error = what;
    return false;
}

std::string parentOf(const std::string& path) {
    const size_t slash = path.find_last_of('/');
    return slash == 0 ? "/" : path.substr(0, slash);
}

bool sameDevice(const std::string& path, dev_t dev) {
    struct stat st {};
    return stat(path.c_str(), &st) == 0 && st.st_dev == dev;
}

// mkdir -p, but only for the trash's own subfolders (0700, the spec wants them private)
bool ensureDir(const std::string& dir) {
    if (mkdir(dir.c_str(), 0700) == 0 || errno == EEXIST) return true;
    if (errno != ENOENT) return false;
    return ensureDir(parentOf(dir)) && (mkdir(dir.c_str(), 0700) == 0 || errno == EEXIST);
}

// walk up while we stay on the same filesystem, that's the mount point ("topdir" in the spec)
std::string mountPointOf(const std::string& path, dev_t dev) {
    std::string cur = parentOf(path);
    while (cur != "/") {
        const std::string up = parentOf(cur);
        if (!sameDevice(up, dev)) break;
        cur = up;
    }
    return cur;
}

std::string percentEncode(const std::string& s) {
    static const char* kHex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        const bool safe = std::isalnum(c) || c == '/' || c == '-' || c == '_' || c == '.' || c == '~';
        if (safe) {
            out += static_cast<char>(c);
        } else {
            out += '%';
            out += kHex[c >> 4];
            out += kHex[c & 0xF];
        }
    }
    return out;
}

struct TrashDir {
    std::string dir;   // .../Trash
    std::string base;  // what Path= in the .trashinfo is relative to ("" = absolute)
};

bool findTrash(const std::string& path, dev_t dev, TrashDir& out) {
    // 1. home trash, if the file lives on the same disk as it
    const char* xdg = std::getenv("XDG_DATA_HOME");
    const char* home = std::getenv("HOME");
    const std::string dataHome = (xdg && *xdg) ? xdg : (home ? std::string(home) + "/.local/share" : "");
    if (!dataHome.empty()) {
        ensureDir(dataHome);
        if (sameDevice(dataHome, dev)) {
            out = {dataHome + "/Trash", ""};
            return true;
        }
    }

    // 2. the disk's own trash
    const std::string top = mountPointOf(path, dev);
    const std::string uid = std::to_string(getuid());
    struct stat st {};
    const std::string shared = (top == "/" ? "" : top) + "/.Trash";
    // an admin-made .Trash only counts if it's a real dir with the sticky bit (spec, for safety)
    if (lstat(shared.c_str(), &st) == 0 && S_ISDIR(st.st_mode) && (st.st_mode & S_ISVTX)) {
        out = {shared + "/" + uid, top};
        if (ensureDir(out.dir)) return true;
    }
    out = {(top == "/" ? "" : top) + "/.Trash-" + uid, top};
    return ensureDir(out.dir);
}

std::string isoNow() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%S", &tm);
    return buf;
}

}  // namespace

bool moveToTrash(const std::string& path, std::string* error) {
    if (path.empty() || path[0] != '/' || path == "/") return fail(error, "refusing to trash " + path);
    struct stat st {};
    if (lstat(path.c_str(), &st) != 0) return fail(error, std::string(std::strerror(errno)));

    TrashDir trash;
    if (!findTrash(path, st.st_dev, trash)) return fail(error, "no trash on that disk we can write to");
    const std::string filesDir = trash.dir + "/files", infoDir = trash.dir + "/info";
    if (!ensureDir(filesDir) || !ensureDir(infoDir)) return fail(error, "can't create " + trash.dir);
    // checked after creating it, a fresh account has no Trash folder yet
    if (!sameDevice(filesDir, st.st_dev)) return fail(error, "trash is on another disk, not copying");

    // the .trashinfo is created with O_EXCL first, that reserves the name (spec says so, avoids races)
    const std::string base = path.substr(path.find_last_of('/') + 1);
    std::string name, infoPath;
    int fd = -1;
    for (int n = 1; n < 10000 && fd < 0; ++n) {
        name = n == 1 ? base : base + "." + std::to_string(n);
        infoPath = infoDir + "/" + name + ".trashinfo";
        fd = open(infoPath.c_str(), O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0600);
        if (fd < 0 && errno != EEXIST) return fail(error, std::string(std::strerror(errno)));
    }
    if (fd < 0) return fail(error, "too many items called " + base + " in the trash");

    // Path= is absolute for the home trash, relative to the mount point for a disk's own trash
    std::string original = path;
    if (!trash.base.empty() && trash.base != "/")
        original = path.substr(trash.base.size() + 1);
    else if (trash.base == "/")
        original = path.substr(1);
    const std::string info = "[Trash Info]\nPath=" + percentEncode(original) + "\nDeletionDate=" + isoNow() + "\n";
    const bool written = write(fd, info.data(), info.size()) == static_cast<ssize_t>(info.size());
    close(fd);

    if (!written || rename(path.c_str(), (filesDir + "/" + name).c_str()) != 0) {
        const std::string why = written ? std::strerror(errno) : "couldn't write the trash info";
        unlink(infoPath.c_str());
        return fail(error, why);
    }
    return true;
}

}  // namespace fe
