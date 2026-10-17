// Milestone 3 (see docs/ROADMAP.md): live updates on Linux.
//
// Plan:
//   1. fanotify_init(FAN_CLASS_NOTIF | FAN_REPORT_DFID_NAME, ...) and
//      fanotify_mark(FAN_MARK_FILESYSTEM, FAN_CREATE | FAN_DELETE | FAN_MOVED_FROM |
//      FAN_MOVED_TO | FAN_ONDIR, ...) per mounted real filesystem.
//      One mark covers a whole filesystem, which is the closest Linux gets to the
//      NTFS USN journal. Needs CAP_SYS_ADMIN (root, or `setcap` on the binary).
//   2. Without that privilege, fall back to inotify: one watch per directory,
//      limited by /proc/sys/fs/inotify/max_user_watches.
//   3. Read events on a background thread, queue them, return them from poll().

#include "platform/Watcher.h"

namespace fe {

std::unique_ptr<Watcher> createWatcher() {
    return nullptr;
}

}  // namespace fe
