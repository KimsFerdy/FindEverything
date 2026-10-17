#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "core/Index.h"

namespace fe {

struct ScanOptions {
    std::vector<std::string> roots;     // e.g. {"/", "/home"}
    std::vector<std::string> excludes;  // dirs left out completely
    bool collectMetadata = true;        // size + mtime, costs a stat() per entry
    // don't walk into other mounts under a root. mounts that are roots themselves get
    // skipped too so they don't show up twice
    bool stayOnFilesystem = false;
};

// shared with the ui thread
struct ScanProgress {
    std::atomic<uint64_t> entries{0};
    std::atomic<uint64_t> errors{0};  // mostly permission denied
    std::atomic<bool> cancel{false};
};

// false if cancelled
bool scanFilesystem(Index& index, const ScanOptions& options, ScanProgress* progress = nullptr);

// /proc, /sys, /dev and every other virtual fs mount
std::vector<std::string> defaultExcludes();

}  // namespace fe
