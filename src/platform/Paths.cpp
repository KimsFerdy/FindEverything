#include "platform/Paths.h"

#include <cstdlib>
#include <filesystem>
#include <system_error>

namespace fe {

namespace fs = std::filesystem;

namespace {

// $XDG_<KIND>_HOME if set, ~/<fallback> otherwise
fs::path xdgDir(const char* var, const char* fallback) {
    const char* xdg = std::getenv(var);
    if (xdg && *xdg) return fs::path(xdg) / "findeverything";
    const char* home = std::getenv("HOME");
    return home ? fs::path(home) / fallback / "findeverything" : fs::path();
}

std::string ensureDir(const fs::path& dir) {
    if (dir.empty()) return {};
    std::error_code ec;
    fs::create_directories(dir, ec);
    return ec ? std::string() : dir.string();
}

std::string fileIn(const std::string& dir, const char* name) {
    return dir.empty() ? std::string() : (fs::path(dir) / name).string();
}

}  // namespace

std::string cacheDirectory() { return ensureDir(xdgDir("XDG_CACHE_HOME", ".cache")); }
std::string configDirectory() { return ensureDir(xdgDir("XDG_CONFIG_HOME", ".config")); }
std::string indexCachePath() { return fileIn(cacheDirectory(), "index.bin"); }
std::string settingsFilePath() { return fileIn(configDirectory(), "settings.ini"); }

std::string executablePath() {
    std::error_code ec;
    fs::path p = fs::read_symlink("/proc/self/exe", ec);
    return ec ? std::string() : p.string();
}

}  // namespace fe
