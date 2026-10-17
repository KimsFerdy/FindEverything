#include "core/Search.h"

#include <algorithm>
#include <cctype>
#include <thread>

namespace fe {

namespace {

// bytes >= 0x80 are utf-8 multibyte chars, so letters in other scripts
bool isWordChar(char c) {
    const auto u = static_cast<unsigned char>(c);
    return std::isalnum(u) || u >= 0x80;
}

bool contains(std::string_view haystack, std::string_view term, bool wholeWord) {
    if (!wholeWord) return haystack.find(term) != std::string_view::npos;
    for (size_t pos = haystack.find(term); pos != std::string_view::npos; pos = haystack.find(term, pos + 1)) {
        const size_t end = pos + term.size();
        const bool startOk = pos == 0 || !isWordChar(haystack[pos - 1]);
        const bool endOk = end == haystack.size() || !isWordChar(haystack[end]);
        if (startOk && endOk) return true;
    }
    return false;
}

// one per thread, owns the scratch buffer for building full paths
class Matcher {
public:
    Matcher(const Index& index, const Query& query, const PathCache* paths)
        : index_(index), q_(query), paths_(query.matchPath ? paths : nullptr) {}

    bool operator()(RecordId id) {
        const FileRecord& r = index_.record(id);
        if (r.flags & kIsDeleted) return false;
        const bool isDir = r.flags & kIsDir;
        if (q_.filesOnly && isDir) return false;
        if (q_.foldersOnly && !isDir) return false;

        std::string_view text = haystack(id);
        for (const std::string& term : q_.include) {
            if (!contains(text, term, q_.wholeWord)) return false;
        }
        for (const std::string& term : q_.exclude) {
            if (contains(text, term, q_.wholeWord)) return false;
        }

        if (!q_.extensions.empty()) {
            if (isDir) return false;
            std::string_view name = index_.lowerName(id);
            size_t dot = name.rfind('.');
            if (dot == std::string_view::npos) return false;
            std::string_view ext = name.substr(dot + 1);
            if (std::find(q_.extensions.begin(), q_.extensions.end(), ext) == q_.extensions.end()) return false;
        }
        return true;
    }

private:
    std::string_view haystack(RecordId id) {
        std::string_view name = q_.matchCase ? index_.name(id) : index_.lowerName(id);
        if (!paths_) return name;
        const RecordId parent = index_.record(id).parent;
        if (parent == kNoParent) return name;

        scratch_.assign(q_.matchCase ? paths_->dirPath(parent) : paths_->lowerDirPath(parent));
        if (!scratch_.empty() && scratch_.back() != kPathSeparator) scratch_ += kPathSeparator;
        scratch_.append(name);
        return scratch_;
    }

    const Index& index_;
    const Query& q_;
    const PathCache* paths_;
    std::string scratch_;
};

}  // namespace

std::vector<RecordId> search(const Index& index, const Query& query, unsigned threads, const PathCache* paths) {
    const size_t n = index.size();
    if (threads == 0) threads = std::max(1u, std::thread::hardware_concurrency());
    if (n < 100'000) threads = 1;  // not worth spinning up threads for small indexes

    // each thread gets its own slice + own output vector, so no locking
    std::vector<std::vector<RecordId>> partial(threads);
    auto work = [&](unsigned t) {
        Matcher matches(index, query, paths);
        const size_t begin = n * t / threads;
        const size_t end = n * (t + 1) / threads;
        for (size_t i = begin; i < end; ++i) {
            if (matches(static_cast<RecordId>(i))) partial[t].push_back(static_cast<RecordId>(i));
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
