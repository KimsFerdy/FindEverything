#pragma once

namespace fe {

// ~/.config/autostart/findeverything.desktop, launched with --minimized.
// shows up in mint's "Startup Applications" too
bool isLaunchAtLoginEnabled();
bool setLaunchAtLogin(bool enable);

}  // namespace fe
