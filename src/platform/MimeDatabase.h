#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fe {

// shared-mime-info, same file -> type table nemo and xdg-open use (/usr/share/mime/globs2).
// name based only.
// TODO(claude): files without an extension could use mime/magic (content sniffing), skipped for now
class MimeDatabase {
public:
    MimeDatabase();

    // "photo.JPG" -> "image/jpeg", "" if unknown
    std::string mimeForName(std::string_view name) const;

    // icon names to try, most specific first: "image/png" -> {"image-png", "image-x-generic"}
    std::vector<std::string> iconNames(const std::string& mime) const;

private:
    std::unordered_map<std::string, std::string> literal_;       // "makefile" -> text/x-makefile
    std::unordered_map<std::string, std::string> suffix_;        // "tar.gz" -> application/x-compressed-tar
    std::vector<std::pair<std::string, std::string>> patterns_;  // everything else, fnmatch()
    std::unordered_map<std::string, std::string> icons_;
    std::unordered_map<std::string, std::string> genericIcons_;
};

// $XDG_DATA_HOME then $XDG_DATA_DIRS
std::vector<std::string> xdgDataDirs();

}  // namespace fe
