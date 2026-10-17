#include "platform/FileIcons.h"

namespace fe {

// Windows (SHGetFileInfo) and macOS (NSWorkspace iconForFile) backends are
// still to come; until then the app draws its own folder/file icons.
std::unique_ptr<FileIcons> createFileIcons(const std::string&) {
    return nullptr;
}

}  // namespace fe
