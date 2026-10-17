#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "platform/IconTheme.h"
#include "platform/MimeDatabase.h"

namespace fe {

// file -> icon file from the desktop's icon theme, so rows look like they do in nemo
class FileIcons {
public:
    explicit FileIcons(const std::string& iconTheme);

    // cheap, called per visible row. it's the mime type ("image/png"), same key = same icon
    std::string iconKey(std::string_view fileName, uint8_t recordFlags, bool isRoot) const;

    // slow-ish (~1ms), once per key. path to a png/svg near sizePx or ""
    std::string iconFile(const std::string& key, int sizePx) const;

private:
    MimeDatabase mime_;
    IconTheme theme_;
};

}  // namespace fe
