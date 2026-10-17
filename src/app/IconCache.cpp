#include "app/IconCache.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

// single header libs, this file holds their implementation. order matters, keep clang-format away
// clang-format off
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"
// clang-format on

namespace fe {

namespace {

struct Rgba {
    int width = 0, height = 0;
    std::vector<uint8_t> pixels;
};

bool decodePng(const std::string& file, Rgba& out) {
    int w, h, channels;
    uint8_t* data = stbi_load(file.c_str(), &w, &h, &channels, 4);
    if (!data) return false;
    out.width = w;
    out.height = h;
    out.pixels.assign(data, data + static_cast<size_t>(w) * h * 4);
    stbi_image_free(data);
    return true;
}

// svgs get rasterized at exactly the size we want
bool decodeSvg(const std::string& file, int sizePx, Rgba& out) {
    NSVGimage* image = nsvgParseFromFile(file.c_str(), "px", 96.0f);
    if (!image) return false;
    bool ok = false;
    if (image->width > 0 && image->height > 0) {
        const float scale = static_cast<float>(sizePx) / std::max(image->width, image->height);
        out.width = out.height = sizePx;
        out.pixels.assign(static_cast<size_t>(sizePx) * sizePx * 4, 0);
        if (NSVGrasterizer* rast = nsvgCreateRasterizer()) {
            const float tx = (sizePx - image->width * scale) * 0.5f;  // center non-square ones
            const float ty = (sizePx - image->height * scale) * 0.5f;
            nsvgRasterize(rast, image, tx, ty, scale, out.pixels.data(), sizePx, sizePx, sizePx * 4);
            nsvgDeleteRasterizer(rast);
            ok = true;
        }
    }
    nsvgDelete(image);
    return ok;
}

}  // namespace

IconCache::IconCache(const std::string& iconTheme, int sizePx) : icons_(iconTheme), size_(sizePx) {}

IconCache::~IconCache() {
    for (const auto& [key, tex] : textures_) {
        if (tex) glDeleteTextures(1, &tex);
    }
}

unsigned IconCache::loadTexture(const std::string& file) const {
    Rgba image;
    const bool isSvg = file.size() > 4 && file.compare(file.size() - 4, 4, ".svg") == 0;
    if (file.empty() || !(isSvg ? decodeSvg(file, size_, image) : decodePng(file, image))) return 0;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 image.pixels.data());
    return tex;
}

ImTextureID IconCache::textureFor(std::string_view fileName, uint8_t recordFlags, bool isRoot) {
    std::string key = icons_.iconKey(fileName, recordFlags, isRoot);
    auto it = textures_.find(key);
    if (it == textures_.end()) {
        it = textures_.emplace(key, loadTexture(icons_.iconFile(key, size_))).first;
    }
    return static_cast<ImTextureID>(static_cast<intptr_t>(it->second));
}

}  // namespace fe
