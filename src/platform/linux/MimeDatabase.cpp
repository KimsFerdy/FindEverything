#include "platform/linux/MimeDatabase.h"

#include <fnmatch.h>

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace fe::linux_fs {

namespace {

std::string toLower(std::string_view s) {
    std::string out(s);
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

// "a:b:c" -> {"a", "b", "c"}
std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> parts;
    std::stringstream ss(s);
    for (std::string part; std::getline(ss, part, sep);) parts.push_back(part);
    return parts;
}

// Reads "key:value" files (mime/icons, mime/generic-icons). First entry wins.
void loadPairs(const std::string& path, std::unordered_map<std::string, std::string>& out) {
    std::ifstream in(path);
    for (std::string line; std::getline(in, line);) {
        const size_t colon = line.find(':');
        if (line.empty() || line[0] == '#' || colon == std::string::npos) continue;
        out.emplace(line.substr(0, colon), line.substr(colon + 1));
    }
}

}  // namespace

std::vector<std::string> xdgDataDirs() {
    std::vector<std::string> dirs;
    const char* home = std::getenv("HOME");
    const char* dataHome = std::getenv("XDG_DATA_HOME");
    if (dataHome && *dataHome) dirs.push_back(dataHome);
    else if (home) dirs.push_back(std::string(home) + "/.local/share");

    const char* dataDirs = std::getenv("XDG_DATA_DIRS");
    for (const std::string& d : split((dataDirs && *dataDirs) ? dataDirs : "/usr/local/share:/usr/share", ':')) {
        if (!d.empty()) dirs.push_back(d);
    }
    return dirs;
}

MimeDatabase::MimeDatabase() {
    // Directories are in priority order, and within globs2 lines are sorted by
    // weight (highest first), so "first one wins" gives the right answer.
    for (const std::string& dir : xdgDataDirs()) {
        std::ifstream in(dir + "/mime/globs2");
        for (std::string line; std::getline(in, line);) {
            if (line.empty() || line[0] == '#') continue;
            std::vector<std::string> f = split(line, ':');  // weight:mime:glob[:flags]
            if (f.size() < 3) continue;
            const std::string& mime = f[1];
            const std::string& glob = f[2];

            const bool hasWildcard = glob.find_first_of("*?[") != std::string::npos;
            if (!hasWildcard) {
                literal_.emplace(toLower(glob), mime);
            } else if (glob.starts_with("*.") && glob.find_first_of("*?[", 2) == std::string::npos) {
                suffix_.emplace(toLower(glob.substr(2)), mime);
            } else {
                patterns_.emplace_back(toLower(glob), mime);
            }
        }
        loadPairs(dir + "/mime/icons", icons_);
        loadPairs(dir + "/mime/generic-icons", genericIcons_);
    }
}

std::string MimeDatabase::mimeForName(std::string_view name) const {
    const std::string lower = toLower(name);

    if (auto it = literal_.find(lower); it != literal_.end()) return it->second;

    // Longest suffix first: "x.tar.gz" tries "tar.gz" before "gz".
    for (size_t dot = lower.find('.'); dot != std::string::npos; dot = lower.find('.', dot + 1)) {
        if (auto it = suffix_.find(lower.substr(dot + 1)); it != suffix_.end()) return it->second;
    }

    for (const auto& [glob, mime] : patterns_) {
        if (fnmatch(glob.c_str(), lower.c_str(), 0) == 0) return mime;
    }
    return {};
}

std::vector<std::string> MimeDatabase::iconNames(const std::string& mime) const {
    std::vector<std::string> names;
    if (auto it = icons_.find(mime); it != icons_.end()) names.push_back(it->second);

    std::string dashed = mime;  // "image/png" -> "image-png" (the spec's default rule)
    for (char& c : dashed) {
        if (c == '/') c = '-';
    }
    names.push_back(dashed);

    if (auto it = genericIcons_.find(mime); it != genericIcons_.end()) names.push_back(it->second);
    const std::string media = mime.substr(0, mime.find('/'));  // "image"
    names.push_back(media + "-x-generic");
    return names;
}

}  // namespace fe::linux_fs
