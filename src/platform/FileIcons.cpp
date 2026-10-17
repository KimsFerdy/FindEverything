#include "platform/FileIcons.h"

#include "core/FileRecord.h"

namespace fe {

namespace {

// fake mime types for stuff that isn't a regular file
constexpr const char* kFolder = "inode/directory";
constexpr const char* kDrive = "inode/mount-point";
constexpr const char* kExecutable = "application/x-executable";
constexpr const char* kUnknown = "application/octet-stream";

}  // namespace

FileIcons::FileIcons(const std::string& iconTheme) : theme_(iconTheme) {}

std::string FileIcons::iconKey(std::string_view fileName, uint8_t flags, bool isRoot) const {
    if (isRoot) return kDrive;
    if (flags & kIsDir) return kFolder;
    std::string mime = mime_.mimeForName(fileName);
    if (!mime.empty()) return mime;
    return (flags & kIsExecutable) ? kExecutable : kUnknown;
}

std::string FileIcons::iconFile(const std::string& key, int sizePx) const {
    std::vector<std::string> names;
    if (key == kFolder)
        names = {"folder"};
    else if (key == kDrive)
        names = {"drive-harddisk", "folder"};
    else if (key == kExecutable)
        names = {"application-x-executable", "application-x-generic"};
    else
        names = mime_.iconNames(key);
    names.push_back("text-x-generic");
    names.push_back("unknown");
    return theme_.find(names, sizePx);
}

}  // namespace fe
