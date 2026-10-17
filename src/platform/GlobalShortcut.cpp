#include "platform/GlobalShortcut.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <vector>

#include "platform/Paths.h"
#include "platform/Process.h"

namespace fe {

namespace {

constexpr const char* kId = "findeverything";

// cinnamon: custom-list holds ids, each id is a relocatable schema under custom-keybindings/<id>/
constexpr const char* kCinnamonList = "org.cinnamon.desktop.keybindings";
constexpr const char* kCinnamonEntry = "org.cinnamon.desktop.keybindings.custom-keybinding";
constexpr const char* kCinnamonPath = "/org/cinnamon/desktop/keybindings/custom-keybindings/findeverything/";
// gnome: same idea, but the list holds full paths and binding is a plain string
constexpr const char* kGnomeList = "org.gnome.settings-daemon.plugins.media-keys";
constexpr const char* kGnomeEntry = "org.gnome.settings-daemon.plugins.media-keys.custom-keybinding";
constexpr const char* kGnomePath = "/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/findeverything/";

// "<exe>" --toggle, quoted for g_shell_parse_argv (what the desktops use to run it)
std::string toggleCommand() {
    std::string out = "\"";
    for (char c : executablePath()) {
        if (c == '"' || c == '\\' || c == '`' || c == '$') out += '\\';
        out += c;
    }
    return out + "\" --toggle";
}

std::string toLower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// gvariant text format string literal
std::string quote(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'' || c == '\\') out += '\\';
        out += c;
    }
    return out + "'";
}

// "@as []" / "['a', 'b']" -> {a, b}
std::vector<std::string> parseStrv(const std::string& text) {
    std::vector<std::string> out;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '\'') continue;
        std::string item;
        for (++i; i < text.size() && text[i] != '\''; ++i) {
            if (text[i] == '\\' && i + 1 < text.size()) ++i;
            item += text[i];
        }
        out.push_back(item);
    }
    return out;
}

std::string formatStrv(const std::vector<std::string>& items) {
    if (items.empty()) return "@as []";
    std::string out = "[";
    for (size_t i = 0; i < items.size(); ++i) out += (i ? ", " : "") + quote(items[i]);
    return out + "]";
}

std::string gget(const std::string& schema, const char* key) {
    std::string out;
    posix::runCapture({"gsettings", "get", schema, key}, &out);
    return out;
}

bool gset(const std::string& schema, const char* key, const std::string& value) {
    return posix::runCapture({"gsettings", "set", schema, key, value}) == 0;
}

bool setCinnamon(const std::string& accel, const std::string& command) {
    const std::string entry = std::string(kCinnamonEntry) + ":" + kCinnamonPath;
    std::vector<std::string> list = parseStrv(gget(kCinnamonList, "custom-list"));
    const bool listed = std::find(list.begin(), list.end(), kId) != list.end();
    if (accel.empty()) {
        if (listed) list.erase(std::remove(list.begin(), list.end(), kId), list.end());
        bool ok = !listed || gset(kCinnamonList, "custom-list", formatStrv(list));
        posix::runCapture({"gsettings", "reset-recursively", entry});
        return ok;
    }
    // cinnamon only reloads custom shortcuts when custom-list changes, edits to an entry that's already
    // listed get ignored (it keeps running the old command). so: take it out, write it, put it back
    if (listed) {
        list.erase(std::remove(list.begin(), list.end(), kId), list.end());
        gset(kCinnamonList, "custom-list", formatStrv(list));
    }
    bool ok = gset(entry, "name", quote("FindEverything")) && gset(entry, "command", quote(command)) &&
              gset(entry, "binding", formatStrv({accel}));
    list.push_back(kId);
    return gset(kCinnamonList, "custom-list", formatStrv(list)) && ok;
}

bool setGnome(const std::string& accel, const std::string& command) {
    const std::string entry = std::string(kGnomeEntry) + ":" + kGnomePath;
    std::vector<std::string> list = parseStrv(gget(kGnomeList, "custom-keybindings"));
    const bool listed = std::find(list.begin(), list.end(), kGnomePath) != list.end();
    if (accel.empty()) {
        if (listed) list.erase(std::remove(list.begin(), list.end(), kGnomePath), list.end());
        bool ok = !listed || gset(kGnomeList, "custom-keybindings", formatStrv(list));
        posix::runCapture({"gsettings", "reset-recursively", entry});
        return ok;
    }
    bool ok = gset(entry, "name", quote("FindEverything")) && gset(entry, "command", quote(command)) &&
              gset(entry, "binding", quote(accel));
    if (ok && !listed) {
        list.push_back(kGnomePath);
        ok = gset(kGnomeList, "custom-keybindings", formatStrv(list));
    }
    return ok;
}

}  // namespace

ShortcutDesktop shortcutDesktop() {
    const char* de = std::getenv("XDG_CURRENT_DESKTOP");
    const std::string desktop = de ? toLower(de) : "";
    if (desktop.find("cinnamon") != std::string::npos) return ShortcutDesktop::Cinnamon;
    if (desktop.find("gnome") != std::string::npos || desktop.find("unity") != std::string::npos ||
        desktop.find("budgie") != std::string::npos)
        return ShortcutDesktop::Gnome;
    return ShortcutDesktop::Unsupported;
}

const char* shortcutDesktopName(ShortcutDesktop desktop) {
    switch (desktop) {
        case ShortcutDesktop::Cinnamon:
            return "Cinnamon";
        case ShortcutDesktop::Gnome:
            return "GNOME";
        case ShortcutDesktop::Unsupported:
            break;
    }
    return "your desktop";
}

bool setGlobalShortcut(const std::string& accel) {
    switch (shortcutDesktop()) {
        case ShortcutDesktop::Cinnamon:
            return setCinnamon(accel, toggleCommand());
        case ShortcutDesktop::Gnome:
            return setGnome(accel, toggleCommand());
        case ShortcutDesktop::Unsupported:
            break;
    }
    return false;
}

std::string shortcutConflict(const std::string& accel) {
    if (accel.empty()) return {};
    const std::vector<const char*> schemas =
        shortcutDesktop() == ShortcutDesktop::Cinnamon
            ? std::vector<const char*>{"org.cinnamon.desktop.keybindings", "org.cinnamon.desktop.keybindings.wm",
                                       "org.cinnamon.desktop.keybindings.media-keys"}
            : std::vector<const char*>{"org.gnome.desktop.wm.keybindings", "org.gnome.shell.keybindings",
                                       "org.gnome.mutter.keybindings", "org.gnome.settings-daemon.plugins.media-keys"};
    const std::string needle = "'" + toLower(accel) + "'";
    for (const char* schema : schemas) {
        std::string out;
        if (posix::runCapture({"gsettings", "list-recursively", schema}, &out) != 0) continue;
        std::istringstream lines(out);
        for (std::string line; std::getline(lines, line);) {
            if (toLower(line).find(needle) == std::string::npos) continue;
            std::istringstream fields(line);  // "<schema> <key> <value>"
            std::string s, key;
            fields >> s >> key;
            if (key != "custom-list" && key != "custom-keybindings") return key;
        }
    }
    return {};
}

std::string prettyAccel(const std::string& accel) {
    std::string out;
    size_t i = 0;
    while (i < accel.size() && accel[i] == '<') {
        const size_t close = accel.find('>', i);
        if (close == std::string::npos) break;
        std::string mod = accel.substr(i + 1, close - i - 1);
        if (mod == "Primary") mod = "Ctrl";
        if (mod == "Control") mod = "Ctrl";
        out += mod + "+";
        i = close + 1;
    }
    std::string key = accel.substr(i);
    if (key.size() == 1) key[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(key[0])));
    if (key == "space") key = "Space";
    return out + key;
}

}  // namespace fe
