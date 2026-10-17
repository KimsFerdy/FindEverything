#include "core/Search.h"

#include <regex.h>

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

bool hasExtension(std::string_view lowerName, const std::vector<std::string>& exts) {
    const size_t dot = lowerName.rfind('.');
    if (dot == std::string_view::npos) return false;
    return std::find(exts.begin(), exts.end(), lowerName.substr(dot + 1)) != exts.end();
}

// one per thread: scratch buffers + its own compiled regexes (glibc locks a shared regex_t while matching)
class Matcher {
public:
    Matcher(const Index& index, const Query& query, const PathCache* paths)
        : index_(index), q_(query), paths_(query.matchPath ? paths : nullptr) {
        const int flags = REG_EXTENDED | REG_NOSUB | (q_.matchCase ? 0 : REG_ICASE);
        for (const std::string& p : q_.includeRegex) regcomp(&include_.emplace_back(), p.c_str(), flags);
        for (const std::string& p : q_.excludeRegex) regcomp(&exclude_.emplace_back(), p.c_str(), flags);
    }
    ~Matcher() {
        for (regex_t& re : include_) regfree(&re);
        for (regex_t& re : exclude_) regfree(&re);
    }
    Matcher(const Matcher&) = delete;
    Matcher& operator=(const Matcher&) = delete;

    // cheap checks first, regex last
    bool operator()(RecordId id) {
        const FileRecord& r = index_.record(id);
        if (r.flags & kIsDeleted) return false;
        const bool isDir = r.flags & kIsDir;
        if (q_.filesOnly && isDir) return false;
        if (q_.foldersOnly && !isDir) return false;
        if (q_.hasSize() && (isDir || r.size < q_.minSize || r.size >= q_.maxSize)) return false;
        if (q_.hasTime() && (r.mtime < q_.minTime || r.mtime >= q_.maxTime)) return false;

        if (!q_.extensions.empty() || !q_.filterExtensions.empty() || q_.filterExecutables) {
            if (isDir) return false;
            const std::string_view name = index_.lowerName(id);
            if (!q_.extensions.empty() && !hasExtension(name, q_.extensions)) return false;
            if (q_.filterExecutables) {
                if (!(r.flags & kIsExecutable) && !hasExtension(name, q_.filterExtensions)) return false;
            } else if (!q_.filterExtensions.empty() && !hasExtension(name, q_.filterExtensions)) {
                return false;
            }
        }

        const std::string_view text = haystack(id);
        for (const std::string& term : q_.include) {
            if (!contains(text, term, q_.wholeWord)) return false;
        }
        for (const std::string& term : q_.exclude) {
            if (contains(text, term, q_.wholeWord)) return false;
        }
        for (const std::string& glob : q_.includeGlobs) {
            if (!globMatch(glob, text)) return false;
        }
        for (const std::string& glob : q_.excludeGlobs) {
            if (globMatch(glob, text)) return false;
        }
        if (!include_.empty() || !exclude_.empty()) {
            regexText_.assign(text);  // regexec wants a c string
            for (const regex_t& re : include_) {
                if (regexec(&re, regexText_.c_str(), 0, nullptr, 0) != 0) return false;
            }
            for (const regex_t& re : exclude_) {
                if (regexec(&re, regexText_.c_str(), 0, nullptr, 0) == 0) return false;
            }
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
    std::string regexText_;
    std::vector<regex_t> include_, exclude_;
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
