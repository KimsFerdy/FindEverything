#pragma once

#include <string>
#include <vector>

namespace fe::linux_fs {

// freedesktop.org Icon Theme spec: a theme is a folder of size-specific
// subfolders described by index.theme, and themes inherit from each other:
//   Mint-Y-Aqua -> Mint-Y -> Adwaita -> gnome -> hicolor
// Try it: grep -A3 '^\[Icon Theme\]' /usr/share/icons/Mint-Y-Aqua/index.theme
class IconTheme {
public:
    explicit IconTheme(const std::string& name);

    // First name (in order) that any theme in the chain has; path to the file
    // whose size is closest to sizePx. "" if none.
    std::string find(const std::vector<std::string>& names, int sizePx) const;

private:
    struct Dir {
        std::string sub;  // "mimetypes/16"
        int size = 0, scale = 1, minSize = 0, maxSize = 0, threshold = 2;
        enum class Type { Fixed, Scalable, Threshold } type = Type::Threshold;
    };
    struct Theme {
        std::vector<std::string> roots;  // "/usr/share/icons/Mint-Y" (a theme can span several base dirs)
        std::vector<Dir> dirs;
    };

    void addTheme(const std::string& name, int depth);
    std::string lookup(const Theme& theme, const std::string& name, int sizePx) const;

    std::vector<std::string> baseDirs_;  // ~/.icons, <data dir>/icons..., /usr/share/pixmaps
    std::vector<std::string> loaded_;    // theme names already in chain_
    std::vector<Theme> chain_;
};

}  // namespace fe::linux_fs
