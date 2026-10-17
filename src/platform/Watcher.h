#pragma once

#include <memory>
#include <string>
#include <vector>

namespace fe {

// Live updates after the initial scan, so the index never goes stale.
// Per-OS backends:
//   Linux   fanotify (whole-filesystem events) with inotify as fallback
//   Windows NTFS USN change journal
//   macOS   FSEvents
struct FsEvent {
    enum class Kind { Created, Deleted, Modified, Renamed };
    Kind kind;
    std::string path;
    std::string newPath;  // only for Renamed
};

class Watcher {
public:
    virtual ~Watcher() = default;
    virtual bool start(const std::vector<std::string>& roots) = 0;
    virtual void stop() = 0;
    // Non-blocking: returns events collected since the last call.
    virtual std::vector<FsEvent> poll() = 0;
};

// Returns nullptr while the platform backend isn't implemented yet.
std::unique_ptr<Watcher> createWatcher();

}  // namespace fe
