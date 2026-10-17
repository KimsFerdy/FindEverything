#pragma once

#include <string>
#include <vector>

namespace fe::linux_fs {

struct MountInfo {
    std::string mountPoint;  // "/home"
    std::string fsType;      // "ext4", "btrfs", "proc", "tmpfs", ...
    std::string source;      // "/dev/nvme0n1p2"
};

// Parses /proc/self/mountinfo — the kernel's live list of mounted filesystems.
std::vector<MountInfo> readMounts();

// True for kernel/virtual filesystems that hold no real user files (proc, sysfs, ...).
bool isPseudoFilesystem(const std::string& fsType);

}  // namespace fe::linux_fs
