#include "platform/Inotify.h"

#include <sys/inotify.h>
#include <unistd.h>

#include <cerrno>
#include <fstream>

namespace fe {

namespace {

constexpr uint32_t kMask = IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_CLOSE_WRITE | IN_ATTRIB |
                           IN_ONLYDIR | IN_DONT_FOLLOW | IN_EXCL_UNLINK;

}  // namespace

Inotify::Inotify() : fd_(inotify_init1(IN_NONBLOCK | IN_CLOEXEC)) {}

Inotify::~Inotify() {
    if (fd_ >= 0) close(fd_);  // drops every watch with it
}

int Inotify::addWatch(const std::string& dir) { return fd_ < 0 ? -1 : inotify_add_watch(fd_, dir.c_str(), kMask); }

void Inotify::removeWatch(int wd) {
    if (fd_ >= 0) inotify_rm_watch(fd_, wd);
}

std::vector<Inotify::Event> Inotify::read() {
    std::vector<Event> events;
    // the buffer has to be aligned for inotify_event
    alignas(inotify_event) char buf[64 * 1024];
    for (;;) {
        const ssize_t n = ::read(fd_, buf, sizeof buf);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            break;  // EAGAIN: queue is empty
        }
        for (char* p = buf; p < buf + n;) {
            const auto* e = reinterpret_cast<const inotify_event*>(p);
            // name is padded with nulls, so build it from the c string not e->len
            events.push_back({e->wd, e->mask, e->cookie, e->len ? std::string(e->name) : std::string()});
            p += sizeof(inotify_event) + e->len;
        }
    }
    return events;
}

size_t Inotify::maxUserWatches() {
    std::ifstream in("/proc/sys/fs/inotify/max_user_watches");
    size_t n = 8192;  // old kernel default
    in >> n;
    return n;
}

}  // namespace fe
