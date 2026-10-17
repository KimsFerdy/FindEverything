#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace fe {

// Finds the image the desktop uses for a file type, so results look like they
// do in the file manager.
class FileIcons {
public:
    virtual ~FileIcons() = default;

    // Cheap; called for every visible row. Identifies the icon for a file
    // (on Linux: its MIME type, e.g. "image/png"). Equal keys = same icon.
    virtual std::string iconKey(std::string_view fileName, uint8_t recordFlags, bool isRoot) const = 0;

    // Expensive; called once per key. Path to a .png or .svg close to sizePx, or "".
    virtual std::string iconFile(const std::string& key, int sizePx) const = 0;
};

// themeHint: the desktop's icon theme name ("Mint-Y-Aqua"); may be empty.
// Returns nullptr where not supported yet (callers draw built-in icons).
std::unique_ptr<FileIcons> createFileIcons(const std::string& themeHint);

}  // namespace fe
