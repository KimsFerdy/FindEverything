#pragma once

#include "core/Index.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace fe {

struct ScanOptions {
    std::vector<std::string> roots;     // where to start, e.g. {"/"}
    std::vector<std::string> excludes;  // absolute directory paths to skip entirely
    bool collectMetadata = true;        // size + mtime; costs one stat() per entry
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
