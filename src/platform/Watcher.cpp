// TODO(claude): live updates, next big thing.
//   fanotify_init(FAN_CLASS_NOTIF | FAN_REPORT_DFID_NAME) + FAN_MARK_FILESYSTEM per disk,
//   needs CAP_SYS_ADMIN. fall back to inotify (one watch per dir, max_user_watches limit)

#include "platform/Watcher.h"

namespace fe {

std::unique_ptr<Watcher> createWatcher() { return nullptr; }

}  // namespace fe
