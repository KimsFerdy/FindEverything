#include "platform/Autostart.h"
#include "platform/Paths.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace fe {

#if defined(_WIN32)

namespace {
constexpr const wchar_t* kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr const wchar_t* kValue = L"FindEverything";
}  // namespace

bool isLaunchAtLoginEnabled() {
    return RegGetValueW(HKEY_CURRENT_USER, kRunKey, kValue, RRF_RT_REG_SZ, nullptr, nullptr, nullptr) ==
           ERROR_SUCCESS;
}

bool setLaunchAtLogin(bool enable) {
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) return false;
    LSTATUS rc;
    if (enable) {
        std::wstring cmd = L"\"" + std::filesystem::path(executablePath()).wstring() + L"\" --minimized";
        rc = RegSetValueExW(key, kValue, 0, REG_SZ, reinterpret_cast<const BYTE*>(cmd.c_str()),
                            static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t)));
    } else {
        rc = RegDeleteValueW(key, kValue);
        if (rc == ERROR_FILE_NOT_FOUND) rc = ERROR_SUCCESS;
    }
    RegCloseKey(key);
    return rc == ERROR_SUCCESS;
}

#elif defined(__APPLE__)

namespace {
std::filesystem::path plistPath() {
    const char* home = std::getenv("HOME");
    return home ? std::filesystem::path(home) / "Library/LaunchAgents/io.github.findeverything.plist"
                : std::filesystem::path();
}
}  // namespace

bool isLaunchAtLoginEnabled() {
    std::error_code ec;
    return std::filesystem::exists(plistPath(), ec);
}

bool setLaunchAtLogin(bool enable) {
    std::error_code ec;
    auto path = plistPath();
    if (path.empty()) return false;
    if (!enable) return std::filesystem::remove(path, ec) || !ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::trunc);
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
           "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
           "<plist version=\"1.0\"><dict>\n"
           "  <key>Label</key><string>io.github.findeverything</string>\n"
           "  <key>ProgramArguments</key><array><string>"
        << executablePath()
        << "</string><string>--minimized</string></array>\n"
           "  <key>RunAtLoad</key><true/>\n"
           "</dict></plist>\n";
    return static_cast<bool>(out.flush());
}

#else

bool isLaunchAtLoginEnabled() { return false; }
bool setLaunchAtLogin(bool) { return false; }

#endif

}  // namespace fe
