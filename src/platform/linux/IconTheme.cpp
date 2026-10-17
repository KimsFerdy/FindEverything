#include "platform/linux/IconTheme.h"
#include "platform/linux/MimeDatabase.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace fe::linux_fs {

namespace fs = std::filesystem;

namespace {

using IniSection = std::unordered_map<std::string, std::string>;

// index.theme is a desktop-entry style INI file: [Section] then Key=Value.
std::unordered_map<std::string, IniSection> readIni(const std::string& path) {
    std::unordered_map<std::string, IniSection> ini;
    std::ifstream in(path);
    IniSection* section = nullptr;
    for (std::string line; std::getline(in, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') {
            section = &ini[line.substr(1, line.size() - 2)];
        } else if (section) {
            const size_t eq = line.find('=');
            if (eq != std::string::npos) (*section)[line.substr(0, eq)] = line.substr(eq + 1);
        }
    }
    return ini;
}

std::vector<std::string> splitList(const std::string& s) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    for (std::string item; std::getline(ss, item, ',');) {
        if (!item.empty()) out.push_back(item);
    }
    return out;
}

int toInt(const IniSection& s, const char* key, int fallback) {
    auto it = s.find(key);
    return it == s.end() ? fallback : std::atoi(it->second.c_str());
}

// We only ever ask for file-type, folder and drive icons. Compared ignoring
// case: the spec says "MimeTypes" but e.g. Mint-Y writes "Mimetypes".
bool usefulContext(const IniSection& s) {
    auto it = s.find("Context");
    if (it == s.end()) return true;
    std::string ctx = it->second;
    for (char& c : ctx) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ctx == "mimetypes" || ctx == "places" || ctx == "devices";
}

}  // namespace

IconTheme::IconTheme(const std::string& name) {
    if (const char* home = std::getenv("HOME")) baseDirs_.push_back(std::string(home) + "/.icons");
    for (const std::string& d : xdgDataDirs()) baseDirs_.push_back(d + "/icons");

    if (!name.empty()) addTheme(name, 0);
    addTheme("hicolor", 0);  // the spec's mandatory final fallback
}

void IconTheme::addTheme(const std::string& name, int depth) {
    if (depth > 10 || std::find(loaded_.begin(), loaded_.end(), name) != loaded_.end()) return;
    loaded_.push_back(name);

    Theme theme;
    std::string indexFile;
    std::error_code ec;
    for (const std::string& base : baseDirs_) {
        const std::string root = base + "/" + name;
        if (!fs::is_directory(root, ec)) continue;
        theme.roots.push_back(root);
        if (indexFile.empty() && fs::exists(root + "/index.theme", ec)) indexFile = root + "/index.theme";
    }
    if (indexFile.empty()) return;

    auto ini = readIni(indexFile);
    const IniSection& header = ini["Icon Theme"];
    auto dirList = header.count("Directories") ? header.at("Directories") : std::string();
    if (header.count("ScaledDirectories")) dirList += "," + header.at("ScaledDirectories");

    for (const std::string& sub : splitList(dirList)) {
        const IniSection& s = ini[sub];
        if (!usefulContext(s)) continue;
        Dir d;
        d.sub = sub;
        d.size = toInt(s, "Size", 0);
        d.scale = toInt(s, "Scale", 1);
        d.minSize = toInt(s, "MinSize", d.size);
        d.maxSize = toInt(s, "MaxSize", d.size);
        d.threshold = toInt(s, "Threshold", 2);
        auto type = s.count("Type") ? s.at("Type") : std::string("Threshold");
        d.type = type == "Fixed" ? Dir::Type::Fixed : type == "Scalable" ? Dir::Type::Scalable : Dir::Type::Threshold;
        if (d.size > 0) theme.dirs.push_back(d);
    }
    chain_.push_back(std::move(theme));

    // Parents come after the theme itself, depth first (as the spec says).
    for (const std::string& parent : splitList(header.count("Inherits") ? header.at("Inherits") : "")) {
        if (parent != "hicolor") addTheme(parent, depth + 1);
    }
}

std::string IconTheme::lookup(const Theme& theme, const std::string& name, int sizePx) const {
    std::string best;
    int bestDistance = INT_MAX;
    std::error_code ec;

    for (const Dir& d : theme.dirs) {
        // How far is this folder's size from what we want? (spec: DirectorySizeDistance)
        int distance;
        const int want = sizePx;
        if (d.type == Dir::Type::Fixed) {
            distance = std::abs(d.size * d.scale - want);
        } else {
            const int lo = (d.type == Dir::Type::Scalable ? d.minSize : d.size - d.threshold) * d.scale;
            const int hi = (d.type == Dir::Type::Scalable ? d.maxSize : d.size + d.threshold) * d.scale;
            distance = want < lo ? lo - want : want > hi ? want - hi : 0;
        }
        if (distance >= bestDistance) continue;

        for (const std::string& root : theme.roots) {
            for (const char* ext : {".png", ".svg"}) {
                std::string path = root + "/" + d.sub + "/" + name + ext;
                if (!fs::exists(path, ec)) continue;
                if (distance == 0) return path;
                best = std::move(path);
                bestDistance = distance;
                break;
            }
        }
    }
    return best;
}

std::string IconTheme::find(const std::vector<std::string>& names, int sizePx) const {
    for (const std::string& name : names) {
        for (const Theme& theme : chain_) {
            std::string path = lookup(theme, name, sizePx);
            if (!path.empty()) return path;
        }
    }
    // Last resort from the spec: unthemed icons in /usr/share/pixmaps.
    std::error_code ec;
    for (const std::string& name : names) {
        for (const char* ext : {".png", ".svg"}) {
            std::string path = "/usr/share/pixmaps/" + name + ext;
            if (fs::exists(path, ec)) return path;
        }
    }
    return {};
}

}  // namespace fe::linux_fs
