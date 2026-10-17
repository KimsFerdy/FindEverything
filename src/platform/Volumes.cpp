#include "platform/Volumes.h"

#include <algorithm>

#include "platform/Mounts.h"

namespace fe {

std::vector<Volume> listVolumes() {
    std::vector<Volume> volumes;
    for (const MountInfo& m : readMounts()) {
        if (isPseudoFilesystem(m.fsType)) continue;
        if (m.fsType == "overlay" || m.fsType.starts_with("fuse.")) continue;  // docker, network/virtual fuse

        // bind mounts show the same partition twice, keep the first one
        const bool duplicate = std::any_of(volumes.begin(), volumes.end(), [&](const Volume& v) {
            return v.device == m.source && m.source.starts_with("/dev/");
        });
        if (duplicate) continue;

        Volume v;
        v.path = m.mountPoint;
        v.device = m.source;
        v.fsType = m.fsType;
        v.removable = m.mountPoint.starts_with("/media/") || m.mountPoint.starts_with("/run/media/");
        volumes.push_back(std::move(v));
    }
    return volumes;
}

}  // namespace fe
