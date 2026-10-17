#pragma once

#include "core/Index.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace fe {

struct ScanOptions {
    std::vector<std::string> roots;     // where to start, e.g. {"/", "/home"}
    std::vector<std::string> excludes;  // absolute directory paths to leave out entirely
    bool collectMetadata = true;        // size + mtime; costs one stat() per entry
    // Don't descend into other filesystems mounted below a root (each disk is
    // its own root instead). Mount points that are themselves roots are skipped
    // so they don't appear twice.
    bool stayOnFilesystem = false;
};

// Shared between the scanning thread and the UI thread, hence atomics.
struct ScanProgress {
    std::atomic<uint64_t> entries{0};
    std::atomic<uint64_t> errors{0};  // usually "permission denied"
    std::atomic<bool> cancel{false};
};

// Implemented once per OS (see CMakeLists.txt for which file is compiled).
// Returns false if cancelled.
bool scanFilesystem(Index& index, const ScanOptions& options, ScanProgress* progress = nullptr);

std::vector<std::string> defaultRoots();
std::vector<std::string> defaultExcludes();

}  // namespace fe
