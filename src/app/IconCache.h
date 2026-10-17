#pragma once

#include <string>
#include <unordered_map>

#include "imgui.h"
#include "platform/FileIcons.h"

namespace fe {

// system file icons as gl textures, one per mime type, loaded on first use.
// needs the gl context alive when created/destroyed
class IconCache {
public:
    IconCache(const std::string& iconTheme, int sizePx);
    ~IconCache();
    IconCache(const IconCache&) = delete;
    IconCache& operator=(const IconCache&) = delete;

    int sizePx() const { return size_; }

    // 0 = no icon found, caller draws a fallback
    ImTextureID textureFor(std::string_view fileName, uint8_t recordFlags, bool isRoot);

private:
    unsigned loadTexture(const std::string& file) const;

    FileIcons icons_;
    int size_;
    std::unordered_map<std::string, unsigned> textures_;  // key -> gl texture (0 = none)
};

}  // namespace fe
