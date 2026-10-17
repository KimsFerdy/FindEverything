#pragma once

namespace fe {

// Launch FindEverything (minimized, via --minimized) when the user logs in.
//   Linux    ~/.config/autostart/findeverything.desktop  (XDG Autostart spec)
//   Windows  HKCU\Software\Microsoft\Windows\CurrentVersion\Run
//   macOS    ~/Library/LaunchAgents/io.github.findeverything.plist
bool isLaunchAtLoginEnabled();
bool setLaunchAtLogin(bool enable);

}  // namespace fe
