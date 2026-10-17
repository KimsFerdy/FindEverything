#pragma once

#include <string>

namespace fe {

// freedesktop.org trash spec: same trash nemo / nautilus / dolphin use, so things can be restored from there.
//   same disk as home   -> $XDG_DATA_HOME/Trash (~/.local/share/Trash)
//   any other disk      -> <mount point>/.Trash/<uid> if an admin set that up, else <mount point>/.Trash-<uid>
// always a rename(), never a copy across disks. false + error if that's not possible
bool moveToTrash(const std::string& path, std::string* error = nullptr);

}  // namespace fe
