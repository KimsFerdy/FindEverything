#include "platform/Volumes.h"
#include "platform/linux/LinuxMounts.h"

#include <algorithm>

namespace fe {

std::vector<Volume> listVolumes() {
    std::vector<Volume> volumes;
    for (const linux_fs::MountInfo& m : linux_fs::readMounts()) {
        if (linux_fs::isPseudoFilesystem(m.fsType)) continue;
        if (m.fsType == "overlay" || m.fsType.starts_with("fuse.")) continue;  // containers, network/virtual FUSE

        // The same partition can be mounted in several places (bind mounts);
        // list it once, at its first (usually main) mount point.
        const bool duplicate = std::any_of(volumes.begin(), volumes.end(), [&](const Volume& v) {
            return v.device == m.source && m.source.starts_with("/dev/");
        });
        if (duplicate) continue;

        Volume v;
        v.path = m.mountPoint;
        v.device = m.source;
        v.fsType = m.fsType;
        // Desktop automounters put USB sticks / SD cards here.
        v.removable = m.mountPoint.starts_with("/media/") || m.mountPoint.starts_with("/run/media/");
        volumes.push_back(std::move(v));
    }
    return volumes;
}

}  // namespace fe
