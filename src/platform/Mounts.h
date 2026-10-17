#pragma once

#include <string>
#include <vector>

namespace fe {

struct MountInfo {
    std::string mountPoint;  // "/home"
    std::string fsType;      // "ext4", "proc", "tmpfs", ...
    std::string source;      // "/dev/nvme0n1p2"
};

// parses /proc/self/mountinfo
std::vector<MountInfo> readMounts();

// proc, sysfs, tmpfs, snap squashfs etc. nothing worth indexing in there
bool isPseudoFilesystem(const std::string& fsType);

}  // namespace fe
