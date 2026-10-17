// fe-cli: index a directory tree, then search it from the terminal.
//
//   fe-cli [--root DIR]... [--limit N] [--no-meta] QUERY...
//   fe-cli --root ~ ext:pdf invoice
//   fe-cli                           (no query: index "/" and print stats)

#include "core/Query.h"
#include "core/Search.h"
#include "platform/Scanner.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

double msSince(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

}  // namespace

int main(int argc, char** argv) {
    fe::ScanOptions opts;
    size_t limit = 50;
    std::string queryText;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--root" && i + 1 < argc) {
            opts.roots.push_back(argv[++i]);
        } else if (arg == "--limit" && i + 1 < argc) {
            limit = std::strtoul(argv[++i], nullptr, 10);
        } else if (arg == "--no-meta") {
            opts.collectMetadata = false;
        } else if (arg == "-h" || arg == "--help") {
            std::puts("usage: fe-cli [--root DIR]... [--limit N] [--no-meta] QUERY...");
            return 0;
        } else {
            if (!queryText.empty()) queryText += ' ';
            queryText += arg;
        }
    }
    if (opts.roots.empty()) opts.roots = fe::defaultRoots();
    opts.excludes = fe::defaultExcludes();

    fe::Index index;
    fe::ScanProgress progress;
    auto t0 = std::chrono::steady_clock::now();
    fe::scanFilesystem(index, opts, &progress);
    std::fprintf(stderr, "indexed %zu entries in %.0f ms (%llu unreadable dirs)\n", index.size(), msSince(t0),
                 static_cast<unsigned long long>(progress.errors.load()));

    if (queryText.empty()) return 0;

    fe::Query query = fe::parseQuery(queryText);
    t0 = std::chrono::steady_clock::now();
    std::vector<fe::RecordId> results = fe::search(index, query);
    double searchMs = msSince(t0);

    for (size_t i = 0; i < results.size() && i < limit; ++i) {
        std::puts(index.fullPath(results[i]).c_str());
    }
    std::fprintf(stderr, "%zu results in %.2f ms%s\n", results.size(), searchMs,
                 results.size() > limit ? " (use --limit to show more)" : "");
    return 0;
}
