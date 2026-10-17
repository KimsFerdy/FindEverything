#include "core/Search.h"

#include <algorithm>
#include <thread>

namespace fe {

bool matches(const Index& index, RecordId id, const Query& q) {
    const FileRecord& r = index.record(id);
    const bool isDir = r.flags & kIsDir;
    if (q.filesOnly && isDir) return false;
    if (q.foldersOnly && !isDir) return false;

    std::string_view name = index.lowerName(id);
    for (const std::string& term : q.include) {
        if (name.find(term) == std::string_view::npos) return false;
    }
    for (const std::string& term : q.exclude) {
        if (name.find(term) != std::string_view::npos) return false;
    }

    if (!q.extensions.empty()) {
        if (isDir) return false;
        size_t dot = name.rfind('.');
        if (dot == std::string_view::npos) return false;
        std::string_view ext = name.substr(dot + 1);
        if (std::find(q.extensions.begin(), q.extensions.end(), ext) == q.extensions.end()) return false;
    }
    return true;
}

std::vector<RecordId> search(const Index& index, const Query& query, unsigned threads) {
    const size_t n = index.size();
    if (threads == 0) threads = std::max(1u, std::thread::hardware_concurrency());
    if (n < 100'000) threads = 1;  // spawning threads costs more than scanning a small index

    // Each thread scans its own contiguous slice into its own vector: no locks needed.
    std::vector<std::vector<RecordId>> partial(threads);
    auto work = [&](unsigned t) {
        const size_t begin = n * t / threads;
        const size_t end = n * (t + 1) / threads;
        for (size_t i = begin; i < end; ++i) {
            if (matches(index, static_cast<RecordId>(i), query)) partial[t].push_back(static_cast<RecordId>(i));
        }
    };

    std::vector<std::thread> pool;
    for (unsigned t = 1; t < threads; ++t) pool.emplace_back(work, t);
    work(0);
    for (std::thread& th : pool) th.join();

    std::vector<RecordId> results;
    size_t total = 0;
    for (const auto& p : partial) total += p.size();
    results.reserve(total);
    for (const auto& p : partial) results.insert(results.end(), p.begin(), p.end());
    return results;
}

}  // namespace fe
