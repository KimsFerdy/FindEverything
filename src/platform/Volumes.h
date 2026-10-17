#pragma once

#include <string>
#include <vector>

namespace fe {

struct Volume {
    std::string path;    // "/", "/home", "/media/me/USB"
    std::string device;  // "/dev/sda2"
    std::string fsType;  // "ext4", "vfat", ...
    bool removable = false;
};

// real disks only, no /proc /sys snaps etc
std::vector<Volume> listVolumes();

}  // namespace fe
