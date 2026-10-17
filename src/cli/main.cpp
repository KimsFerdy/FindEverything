// fe-cli ext:pdf invoice            uses the gui's settings + cached index
// fe-cli --rescan                   rescan and refresh the cache
// fe-cli --root ~ ext:pdf invoice   just ~, never cached

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "app/Settings.h"
#include "core/IndexFile.h"
#include "core/PathCache.h"
#include "core/Query.h"
#include "core/Search.h"
#include "platform/Paths.h"
#include "platform/Scanner.h"

namespace {

double msSince(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

}  // namespace

int main(int argc, char** argv) {
    fe::ScanOptions opts;
    fe::SearchOptions searchOpts;
    size_t limit = 50;
    bool rescan = false;
    std::string queryText;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--root" && i + 1 < argc) {
            opts.roots.push_back(argv[++i]);
        } else if (arg == "--limit" && i + 1 < argc) {
            limit = std::strtoul(argv[++i], nullptr, 10);
        } else if (arg == "--rescan") {
            rescan = true;
        } else if (arg == "--no-meta") {
            opts.collectMetadata = false;
        } else if (arg == "--case") {
            searchOpts.matchCase = true;
        } else if (arg == "--word") {
            searchOpts.wholeWord = true;
        } else if (arg == "--path") {
            searchOpts.matchPath = true;
        } else if (arg == "-h" || arg == "--help") {
            std::puts(
                "usage: fe-cli [--root DIR]... [--rescan] [--limit N] [--no-meta] [--case] [--word] [--path] QUERY...");
            return 0;
        } else {
            if (!queryText.empty()) queryText += ' ';
            queryText += arg;
        }
    }
    // no --root = same disks/excludes as the gui, so we can share its cache
    const bool useCache = opts.roots.empty();
    std::string source;
    if (useCache) {
        const fe::Settings settings = fe::loadSettings();
        const bool collectMetadata = opts.collectMetadata;
        opts = fe::scanOptionsFor(settings);
        opts.collectMetadata = collectMetadata;
        source = fe::scanSourceFor(opts, settings);
    } else {
        opts.excludes = fe::defaultExcludes();
    }
    const std::string cachePath = useCache ? fe::indexCachePath() : "";

    fe::Index index;
    auto t0 = std::chrono::steady_clock::now();
    fe::IndexFileInfo info;
    if (!cachePath.empty() && !rescan && fe::loadIndex(index, cachePath, &info) && info.source == source) {
        std::fprintf(stderr, "loaded %zu entries from %s in %.0f ms\n", index.size(), cachePath.c_str(), msSince(t0));
    } else {
        fe::ScanProgress progress;
        fe::scanFilesystem(index, opts, &progress);
        std::fprintf(stderr, "indexed %zu entries in %.0f ms (%llu unreadable dirs)\n", index.size(), msSince(t0),
                     static_cast<unsigned long long>(progress.errors.load()));
        if (!cachePath.empty() && fe::saveIndex(index, cachePath, source)) {
            std::fprintf(stderr, "saved index to %s\n", cachePath.c_str());
        }
    }

    if (queryText.empty()) return 0;

    fe::Query query = fe::parseQuery(queryText, searchOpts);
    fe::PathCache paths;
    if (query.matchPath) paths.build(index);
    t0 = std::chrono::steady_clock::now();
    std::vector<fe::RecordId> results = fe::search(index, query, 0, &paths);
    double searchMs = msSince(t0);

    for (size_t i = 0; i < results.size() && i < limit; ++i) {
        std::puts(index.fullPath(results[i]).c_str());
    }
    std::fprintf(stderr, "%zu results in %.2f ms%s\n", results.size(), searchMs,
                 results.size() > limit ? " (use --limit to show more)" : "");
    return 0;
}
