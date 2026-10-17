#pragma once

#include <memory>
#include <string>
#include <vector>

namespace fe {

// live updates after the first scan so the index doesn't go stale.
// plan: fanotify (whole filesystem marks) when we have the caps, inotify otherwise
struct FsEvent {
    enum class Kind { Created, Deleted, Modified, Renamed };
    Kind kind;
    std::string path;
    std::string newPath;  // Renamed only
};

class Watcher {
public:
    virtual ~Watcher() = default;
    virtual bool start(const std::vector<std::string>& roots) = 0;
    virtual void stop() = 0;
    virtual std::vector<FsEvent> poll() = 0;  // non blocking, whatever came in since last call
};

// nullptr until it's written
std::unique_ptr<Watcher> createWatcher();

}  // namespace fe
