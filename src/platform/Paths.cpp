#include "platform/Paths.h"

#include <cstdlib>
#include <filesystem>
#include <system_error>

namespace fe {

namespace fs = std::filesystem;

namespace {

std::string env(const char* name) {
    const char* v = std::getenv(name);
    return v ? v : "";
}

fs::path cacheBase() {
#if defined(_WIN32)
    std::string local = env("LOCALAPPDATA");
    return local.empty() ? fs::path() : fs::path(local) / "FindEverything";
#elif defined(__APPLE__)
    std::string home = env("HOME");
    return home.empty() ? fs::path() : fs::path(home) / "Library/Caches/FindEverything";
#else
    // XDG Base Directory spec: honour $XDG_CACHE_HOME, fall back to ~/.cache.
    std::string xdg = env("XDG_CACHE_HOME");
    if (!xdg.empty()) return fs::path(xdg) / "findeverything";
    std::string home = env("HOME");
    return home.empty() ? fs::path() : fs::path(home) / ".cache/findeverything";
#endif
}

}  // namespace

std::string cacheDirectory() {
    fs::path dir = cacheBase();
    if (dir.empty()) return {};
    std::error_code ec;
    fs::create_directories(dir, ec);
    return ec ? std::string() : dir.string();
}

std::string indexCachePath() {
    std::string dir = cacheDirectory();
    return dir.empty() ? std::string() : (fs::path(dir) / "index.bin").string();
}

}  // namespace fe
