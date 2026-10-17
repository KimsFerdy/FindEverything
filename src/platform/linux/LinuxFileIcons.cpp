#include "core/FileRecord.h"
#include "platform/FileIcons.h"
#include "platform/linux/IconTheme.h"
#include "platform/linux/MimeDatabase.h"

namespace fe {

namespace {

// Pseudo MIME types for things that aren't regular files.
constexpr const char* kFolder = "inode/directory";
constexpr const char* kDrive = "inode/mount-point";
constexpr const char* kExecutable = "application/x-executable";
constexpr const char* kUnknown = "application/octet-stream";

class LinuxFileIcons final : public FileIcons {
public:
    explicit LinuxFileIcons(const std::string& theme) : theme_(theme) {}

    std::string iconKey(std::string_view fileName, uint8_t flags, bool isRoot) const override {
        if (isRoot) return kDrive;
        if (flags & kIsDir) return kFolder;
        std::string mime = mime_.mimeForName(fileName);
        if (!mime.empty()) return mime;
        return (flags & kIsExecutable) ? kExecutable : kUnknown;
    }

    std::string iconFile(const std::string& key, int sizePx) const override {
        std::vector<std::string> names;
        if (key == kFolder) names = {"folder"};
        else if (key == kDrive) names = {"drive-harddisk", "folder"};
        else if (key == kExecutable) names = {"application-x-executable", "application-x-generic"};
        else names = mime_.iconNames(key);
        names.push_back("text-x-generic");  // the spec's suggested ultimate fallback
        names.push_back("unknown");
        return theme_.find(names, sizePx);
    }

private:
    linux_fs::MimeDatabase mime_;
    linux_fs::IconTheme theme_;
};

}  // namespace

std::unique_ptr<FileIcons> createFileIcons(const std::string& themeHint) {
    return std::make_unique<LinuxFileIcons>(themeHint);
}

}  // namespace fe
