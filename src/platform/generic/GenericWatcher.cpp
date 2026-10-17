#include "platform/Watcher.h"

namespace fe {

// No portable live-update mechanism exists; see src/platform/windows and src/platform/macos.
std::unique_ptr<Watcher> createWatcher() {
    return nullptr;
}

}  // namespace fe
