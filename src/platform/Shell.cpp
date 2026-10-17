#include "platform/Shell.h"

#include "platform/Process.h"

namespace fe {

namespace {

// "/home/me/My File.txt" -> "file:///home/me/My%20File.txt"
std::string toFileUri(const std::string& path) {
    static const char* kHex = "0123456789ABCDEF";
    std::string uri = "file://";
    for (unsigned char c : path) {
        const bool safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '/' ||
                          c == '-' || c == '_' || c == '.' || c == '~';
        if (safe) {
            uri += static_cast<char>(c);
        } else {
            uri += '%';
            uri += kHex[c >> 4];
            uri += kHex[c & 0xF];
        }
    }
    return uri;
}

std::string parentDir(const std::string& path) {
    size_t slash = path.find_last_of('/');
    if (slash == std::string::npos) return ".";
    return slash == 0 ? "/" : path.substr(0, slash);
}

}  // namespace

bool openPath(const std::string& path) { return posix::spawnDetached({"xdg-open", path}); }

bool revealInFileManager(const std::string& path) {
    // org.freedesktop.FileManager1, nemo / nautilus / dolphin / thunar all do this.
    // commas are percent-encoded in the uri which matters since dbus-send splits arrays on ','
    int rc =
        posix::runCapture({"dbus-send", "--session", "--print-reply", "--reply-timeout=2000",
                           "--dest=org.freedesktop.FileManager1", "/org/freedesktop/FileManager1",
                           "org.freedesktop.FileManager1.ShowItems", "array:string:" + toFileUri(path), "string:"});
    if (rc == 0) return true;
    return posix::spawnDetached({"xdg-open", parentDir(path)});  // no FileManager1, just open the folder
}

}  // namespace fe
