#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fe::linux_fs {

// freedesktop.org Shared MIME-info database: the same file -> type table Nemo,
// Nautilus and xdg-open use. Compiled by `update-mime-database` into
// /usr/share/mime/globs2 (lines like "50:image/png:*.png").
// Only file names are used (no peeking at file contents).
class MimeDatabase {
public:
    MimeDatabase();  // reads every mime/ dir in $XDG_DATA_HOME and $XDG_DATA_DIRS

    // "photo.JPG" -> "image/jpeg"; "" if unknown.
    std::string mimeForName(std::string_view name) const;

    // Icon names to try for a MIME type, most specific first:
    // "image/png" -> {"image-png", "image-x-generic"}.
    std::vector<std::string> iconNames(const std::string& mime) const;

private:
    std::unordered_map<std::string, std::string> literal_;  // "makefile" -> text/x-makefile
    std::unordered_map<std::string, std::string> suffix_;   // "tar.gz" -> application/x-compressed-tar
    std::vector<std::pair<std::string, std::string>> patterns_;  // other globs, matched with fnmatch()
    std::unordered_map<std::string, std::string> icons_;         // mime -> icon (mime/icons)
    std::unordered_map<std::string, std::string> genericIcons_;  // mime -> generic icon (mime/generic-icons)
};

// $XDG_DATA_HOME followed by $XDG_DATA_DIRS, with spec defaults.
std::vector<std::string> xdgDataDirs();

}  // namespace fe::linux_fs
