#pragma once

#include <string>
#include <vector>

namespace fe {

// A mounted disk/partition the user can choose to index.
struct Volume {
    std::string path;       // "/", "/home", "/media/kims/USB", "C:\"
    std::string device;     // "/dev/sda2", or a drive label on Windows
    std::string fsType;     // "ext4", "ntfs", "vfat", ...
    bool removable = false;
};

// Real, user-relevant filesystems only (no /proc, /sys, snap images, ...).
std::vector<Volume> listVolumes();

}  // namespace fe
