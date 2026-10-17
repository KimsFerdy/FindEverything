#pragma once

#include <string>

namespace fe {

// global shortcut = a custom keyboard shortcut in the desktop's own settings that runs
// `FindEverything --toggle`. no key grabbing of our own, so it shows up (and can be edited) in
// the desktop's keyboard settings and keeps working on wayland
enum class ShortcutDesktop { Cinnamon, Gnome, Unsupported };

ShortcutDesktop shortcutDesktop();
const char* shortcutDesktopName(ShortcutDesktop desktop);

// accel in gtk format ("<Super>f"), "" removes our shortcut. false if gsettings refused.
// the command is this binary with --toggle
bool setGlobalShortcut(const std::string& accel);

// name of an existing desktop binding that already uses accel, or ""
std::string shortcutConflict(const std::string& accel);

// "<Super><Shift>f" -> "Super+Shift+F"
std::string prettyAccel(const std::string& accel);

}  // namespace fe
