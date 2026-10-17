// XDG Autostart: at login, the desktop session (Cinnamon, GNOME, KDE...) runs
// every *.desktop file in ~/.config/autostart. You can see and toggle these in
// Mint's "Startup Applications" too; ours will show up there.

#include "platform/Autostart.h"
#include "platform/Paths.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace fe {

namespace fs = std::filesystem;

namespace {

fs::path desktopFile() {
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    const char* home = std::getenv("HOME");
    fs::path base = (xdg && *xdg) ? fs::path(xdg) : (home ? fs::path(home) / ".config" : fs::path());
    return base.empty() ? fs::path() : base / "autostart" / "findeverything.desktop";
}

// Desktop-entry Exec values need spaces, quotes and backslashes quoted.
std::string quoteExec(const std::string& path) {
    std::string out = "\"";
    for (char c : path) {
        if (c == '"' || c == '\\' || c == '`' || c == '$') out += '\\';
        out += c;
    }
    return out + "\"";
}

}  // namespace

bool isLaunchAtLoginEnabled() {
    std::error_code ec;
    fs::path file = desktopFile();
    return !file.empty() && fs::exists(file, ec);
}

bool setLaunchAtLogin(bool enable) {
    fs::path file = desktopFile();
    if (file.empty()) return false;
    std::error_code ec;
    if (!enable) {
        fs::remove(file, ec);
        return !ec;
    }

    const std::string exe = executablePath();
    if (exe.empty()) return false;
    fs::create_directories(file.parent_path(), ec);
    std::ofstream out(file, std::ios::trunc);
    out << "[Desktop Entry]\n"
           "Type=Application\n"
           "Name=FindEverything\n"
           "Comment=Instant file search\n"
           "Exec="
        << quoteExec(exe)
        << " --minimized\n"
           "Terminal=false\n"
           "X-GNOME-Autostart-enabled=true\n";
    return static_cast<bool>(out.flush());
}

}  // namespace fe
