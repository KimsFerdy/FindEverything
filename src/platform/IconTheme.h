#pragma once

#include <string>
#include <vector>

namespace fe {

// freedesktop icon theme lookup. themes inherit, e.g.
//   Mint-Y-Aqua -> Mint-Y -> Adwaita -> gnome -> hicolor
class IconTheme {
public:
    explicit IconTheme(const std::string& name);

    // first name any theme in the chain has, closest size wins. "" if nothing
    std::string find(const std::vector<std::string>& names, int sizePx) const;

private:
    struct Dir {
        std::string sub;  // "mimetypes/16"
        int size = 0, scale = 1, minSize = 0, maxSize = 0, threshold = 2;
        enum class Type { Fixed, Scalable, Threshold } type = Type::Threshold;
    };
    struct Theme {
        std::vector<std::string> roots;  // one theme can live in several base dirs
        std::vector<Dir> dirs;
    };

    void addTheme(const std::string& name, int depth);
    std::string lookup(const Theme& theme, const std::string& name, int sizePx) const;

    std::vector<std::string> baseDirs_;
    std::vector<std::string> loaded_;
    std::vector<Theme> chain_;
};

}  // namespace fe
