#include "platform/Paths.h"

#include <cstdlib>
#include <filesystem>
#include <system_error>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace fe {

namespace fs = std::filesystem;

namespace {

std::string env(const char* name) {
    const char* v = std::getenv(name);
    return v ? v : "";
}

// XDG Base Directory spec: honour $XDG_<KIND>_HOME, fall back to ~/<fallback>.
fs::path xdgDir(const char* var, const char* fallback) {
    std::string xdg = env(var);
    if (!xdg.empty()) return fs::path(xdg) / "findeverything";
    std::string home = env("HOME");
    return home.empty() ? fs::path() : fs::path(home) / fallback / "findeverything";
}

fs::path cacheBase() {
#if defined(_WIN32)
    std::string local = env("LOCALAPPDATA");
    return local.empty() ? fs::path() : fs::path(local) / "FindEverything";
#elif defined(__APPLE__)
    std::string home = env("HOME");
    return home.empty() ? fs::path() : fs::path(home) / "Library/Caches/FindEverything";
#else
    return xdgDir("XDG_CACHE_HOME", ".cache");
#endif
}

fs::path configBase() {
#if defined(_WIN32)
    std::string roaming = env("APPDATA");
    return roaming.empty() ? fs::path() : fs::path(roaming) / "FindEverything";
#elif defined(__APPLE__)
    std::string home = env("HOME");
    return home.empty() ? fs::path() : fs::path(home) / "Library/Application Support/FindEverything";
#else
    return xdgDir("XDG_CONFIG_HOME", ".config");
#endif
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

std::string cacheDirectory() { return ensureDir(cacheBase()); }
std::string configDirectory() { return ensureDir(configBase()); }
std::string indexCachePath() { return fileIn(cacheDirectory(), "index.bin"); }
std::string settingsFilePath() { return fileIn(configDirectory(), "settings.ini"); }

std::string executablePath() {
    std::error_code ec;
#if defined(_WIN32)
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return n ? fs::path(std::wstring(buf, n)).string() : std::string();
#elif defined(__APPLE__)
    char buf[4096];
    uint32_t size = sizeof buf;
    if (_NSGetExecutablePath(buf, &size) != 0) return {};
    fs::path p = fs::canonical(buf, ec);
    return ec ? std::string(buf) : p.string();
#else
    // /proc/self/exe is a symlink the kernel maintains to our own binary.
    fs::path p = fs::read_symlink("/proc/self/exe", ec);
    return ec ? std::string() : p.string();
#endif
}

}  // namespace fe
