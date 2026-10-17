#pragma once

#include "platform/FileIcons.h"

#include "imgui.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace fe {

// System file-type icons as GPU textures, loaded on first use and kept for the
// session. One texture per icon key (MIME type), so 500k files cost a few dozen
// textures. Must be created and destroyed with the OpenGL context current.
class IconCache {
public:
    IconCache(const std::string& iconTheme, int sizePx);
    ~IconCache();
    IconCache(const IconCache&) = delete;
    IconCache& operator=(const IconCache&) = delete;

    int sizePx() const { return size_; }

    // Texture for this file, or 0 (no system icon: draw a built-in one).
    ImTextureID textureFor(std::string_view fileName, uint8_t recordFlags, bool isRoot);

private:
    unsigned loadTexture(const std::string& file) const;

    std::unique_ptr<FileIcons> icons_;
    int size_;
    std::unordered_map<std::string, unsigned> textures_;  // icon key -> GL texture (0 = none)
};

}  // namespace fe
