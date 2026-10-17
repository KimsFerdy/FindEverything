#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fe {

// thin raii wrapper around inotify. one watch per directory, the kernel caps how many
// a user gets (/proc/sys/fs/inotify/max_user_watches) and every app shares that budget
class Inotify {
public:
    struct Event {
        int wd;
        uint32_t mask;
        uint32_t cookie;   // pairs IN_MOVED_FROM with IN_MOVED_TO
        std::string name;  // entry inside the watched dir, empty for events on the dir itself
    };

    Inotify();
    ~Inotify();
    Inotify(const Inotify&) = delete;
    Inotify& operator=(const Inotify&) = delete;

    bool ok() const { return fd_ >= 0; }
    int fd() const { return fd_; }  // for poll()

    int addWatch(const std::string& dir);  // -1 on failure
    void removeWatch(int wd);

    // non blocking, everything that's queued right now
    std::vector<Event> read();

    static size_t maxUserWatches();

private:
    int fd_ = -1;
};

}  // namespace fe
