#include "core/Sort.h"

#include <algorithm>
#include <thread>

namespace fe {

namespace {

// first 8 bytes packed big-endian into a u64, compares the same as the bytes do.
// most compares end here without touching the strings. (claude's idea, went from ~1.2s to ~0.6s)
uint64_t prefixKey(std::string_view s) {
    uint64_t key = 0;
    for (size_t i = 0; i < 8; ++i) {
        key <<= 8;
        if (i < s.size()) key |= static_cast<unsigned char>(s[i]);
    }
    return key;
}

// sort chunks on separate threads, then merge neighbours pairwise until one is left
template <typename T, typename Less>
void parallelSort(std::vector<T>& v, Less less) {
    const size_t threads = std::clamp<size_t>(std::thread::hardware_concurrency(), 1, 8);
    if (threads == 1 || v.size() < 50'000) {
        std::sort(v.begin(), v.end(), less);
        return;
    }
    std::vector<size_t> bounds;
    for (size_t t = 0; t <= threads; ++t) bounds.push_back(v.size() * t / threads);

    std::vector<std::thread> pool;
    for (size_t t = 0; t < threads; ++t) {
        pool.emplace_back([&, t] { std::sort(v.begin() + bounds[t], v.begin() + bounds[t + 1], less); });
    }
    for (std::thread& th : pool) th.join();

    for (size_t width = 1; width < threads; width *= 2) {
        pool.clear();
        for (size_t t = 0; t + width < threads; t += 2 * width) {
            const size_t lo = bounds[t], mid = bounds[t + width], hi = bounds[std::min(t + 2 * width, threads)];
            pool.emplace_back(
                [&, lo, mid, hi] { std::inplace_merge(v.begin() + lo, v.begin() + mid, v.begin() + hi, less); });
        }
        for (std::thread& th : pool) th.join();
    }
}

// by string: lowercase first, then exact case, then id so the order is stable between rescans
template <typename GetLower, typename GetExact>
void sortByString(std::vector<RecordId>& ids, GetLower getLower, GetExact getExact) {
    struct Entry {
        uint64_t key;
        RecordId id;
    };
    std::vector<Entry> entries;
    entries.reserve(ids.size());
    for (RecordId id : ids) entries.push_back({prefixKey(getLower(id)), id});

    parallelSort(entries, [&](const Entry& a, const Entry& b) {
        if (a.key != b.key) return a.key < b.key;
        if (int c = getLower(a.id).compare(getLower(b.id))) return c < 0;
        if (int c = getExact(a.id).compare(getExact(b.id))) return c < 0;
        return a.id < b.id;
    });
    for (size_t i = 0; i < entries.size(); ++i) ids[i] = entries[i].id;
}

// by (primary, nameRank), all integers
template <typename Primary>
std::vector<RecordId> sortByNumber(size_t n, const std::vector<uint32_t>& nameRank, Primary primary) {
    struct Entry {
        decltype(primary(RecordId{})) key;
        uint32_t rank;
        RecordId id;
    };
    std::vector<Entry> entries(n);
    for (RecordId id = 0; id < n; ++id) entries[id] = {primary(id), nameRank[id], id};
    std::sort(entries.begin(), entries.end(),
              [](const Entry& a, const Entry& b) { return a.key != b.key ? a.key < b.key : a.rank < b.rank; });
    std::vector<RecordId> out(n);
    for (size_t i = 0; i < n; ++i) out[i] = entries[i].id;
    return out;
}

}  // namespace

void SortOrders::build(const Index& index, const PathCache& paths) {
    const size_t n = index.size();
    auto lowerName = [&](RecordId id) { return index.lowerName(id); };
    auto exactName = [&](RecordId id) { return index.name(id); };

    // step 1, in parallel: folder names, file names, folder paths (the only string sorts)
    std::vector<RecordId> dirs, files;
    for (RecordId id = 0; id < n; ++id) ((index.record(id).flags & kIsDir) ? dirs : files).push_back(id);
    std::vector<RecordId> dirsByPath = dirs;

    std::thread sortFiles([&] { sortByString(files, lowerName, exactName); });
    std::thread sortDirPaths([&] {
        sortByString(
            dirsByPath, [&](RecordId id) { return paths.lowerDirPath(id); },
            [&](RecordId id) { return paths.dirPath(id); });
    });
    sortByString(dirs, lowerName, exactName);
    sortFiles.join();
    sortDirPaths.join();

    // folders first, like Everything
    std::vector<RecordId>& byName = orders_[static_cast<int>(SortKey::Name)];
    byName = std::move(dirs);
    byName.insert(byName.end(), files.begin(), files.end());

    // ranks so everything after this compares ints instead of strings
    std::vector<uint32_t> nameRank(n), dirRank(n, 0);
    for (size_t i = 0; i < n; ++i) nameRank[byName[i]] = static_cast<uint32_t>(i);
    for (size_t i = 0; i < dirsByPath.size(); ++i) dirRank[dirsByPath[i]] = static_cast<uint32_t>(i + 1);

    // step 2, in parallel: the rest
    auto parentRank = [&](RecordId id) {
        const RecordId parent = index.record(id).parent;
        return parent == kNoParent ? uint32_t{0} : dirRank[parent];  // roots go first
    };
    auto size = [&](RecordId id) { return index.record(id).size; };
    auto mtime = [&](RecordId id) { return index.record(id).mtime; };

    std::thread t1([&] { orders_[static_cast<int>(SortKey::Path)] = sortByNumber(n, nameRank, parentRank); });
    std::thread t2([&] { orders_[static_cast<int>(SortKey::Size)] = sortByNumber(n, nameRank, size); });
    orders_[static_cast<int>(SortKey::Modified)] = sortByNumber(n, nameRank, mtime);
    t1.join();
    t2.join();
}

std::vector<RecordId> sortResults(const std::vector<RecordId>& results, size_t indexSize, const SortOrders& sorts,
                                  SortKey key, bool descending) {
    const std::vector<RecordId>& order = sorts.order(key);
    if (order.size() != indexSize) return results;  // not built for this index

    std::vector<RecordId> out;
    if (results.size() == indexSize) {  // empty query, everything matches
        out = order;
    } else {
        // mark matches in a bitmap (64KB for 500k), walk the sorted order, keep the marked ones
        std::vector<uint64_t> bits((indexSize + 63) / 64);
        for (RecordId id : results) bits[id >> 6] |= uint64_t{1} << (id & 63);
        out.reserve(results.size());
        for (RecordId id : order) {
            if ((bits[id >> 6] >> (id & 63)) & 1) out.push_back(id);
        }
    }
    if (descending) std::reverse(out.begin(), out.end());
    return out;
}

}  // namespace fe
